/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "VCSM.h"

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

using namespace vc4cl;

#ifdef __cplusplus
extern "C"
{
#endif

    // see "/opt/vc/include/interface/vcsm/user-vcsm.h"
    typedef enum
    {
        VCSM_CACHE_TYPE_NONE = 0,     // No caching applies.
        VCSM_CACHE_TYPE_HOST,         // Allocation is cached on host (user space).
        VCSM_CACHE_TYPE_VC,           // Allocation is cached on videocore.
        VCSM_CACHE_TYPE_HOST_AND_VC,  // Allocation is cached on both host and videocore.
        VCSM_CACHE_TYPE_PINNED = 0x80 // Pre-pin (vs. allocation on first access), see
                                      // https://github.com/xbmc/xbmc/commit/3d1e2d73dbbcbc8c1e6bf63f7e563ae96312c394

    } VCSM_CACHE_TYPE_T;

    int vcsm_init_ex(int want_cma, int fd);
    void vcsm_exit(void);

    unsigned int vcsm_malloc_cache(unsigned int size, VCSM_CACHE_TYPE_T cache, const char* name);
    void vcsm_free(unsigned int handle);

    unsigned int vcsm_vc_addr_from_hdl(unsigned int handle);

    void* vcsm_lock(unsigned int handle);
    int vcsm_unlock_ptr_sp(void* usr_ptr, int cache_no_flush);

    int vcsm_export_dmabuf(unsigned int vcsm_handle);

    struct vcsm_user_clean_invalid2_s
    {
        unsigned char op_count;
        unsigned char zero[3];
        struct vcsm_user_clean_invalid2_block_s
        {
            unsigned short invalidate_mode;
            unsigned short block_count;
            void* start_address;
            unsigned int block_size;
            unsigned int inter_block_stride;
        } s[0];
    };

    int vcsm_clean_invalid2(struct vcsm_user_clean_invalid2_s* s);

// Taken from kernel header "linux/drivers/staging/vc04_services/include/linux/broadcom/vc_sm_cma_ioctl.h "
/*
 * Cache functions to be set to struct vc_sm_cma_ioctl_clean_invalid2 invalidate_mode.
 */
#define VC_SM_CACHE_OP_NOP 0x00
#define VC_SM_CACHE_OP_INV 0x01
#define VC_SM_CACHE_OP_CLEAN 0x02
#define VC_SM_CACHE_OP_FLUSH 0x03

#ifdef __cplusplus
}
#endif

VCSM::VCSM(bool useCMA)
{
    if(vcsm_init_ex(useCMA, -1) != 0)
        throw std::runtime_error("Failed to initialize VCSM!");

    // The VCSM library falls back to the other interface (see vcsm_init_ex, e.g. from CMA to non-CMA, if CMA is not
    // available). So we check whether the desired interface is available or not.
    usesCMA = !useCMA;
    int fd = open(useCMA ? "/dev/vcsm-cma" : "/dev/vcsm", O_RDWR, 0);
    if(fd >= 0)
    {
        usesCMA = useCMA;
        close(fd);
    }
    DEBUG_LOG(
        DebugLevel::SYSCALL, std::cout << "[VC4CL] VCSM initialized: " << (usesCMA ? "vcsm-cma" : "vcsm") << std::endl)
}

VCSM::~VCSM()
{
    vcsm_exit();
    DEBUG_LOG(DebugLevel::SYSCALL, std::cout << "[VC4CL] VCSM shut down" << std::endl)
}

static VCSM_CACHE_TYPE_T toCacheType(CacheType type)
{
    switch(type)
    {
    case CacheType::UNCACHED:
        return static_cast<VCSM_CACHE_TYPE_T>(VCSM_CACHE_TYPE_PINNED | VCSM_CACHE_TYPE_NONE);
    case CacheType::HOST_CACHED:
        return static_cast<VCSM_CACHE_TYPE_T>(VCSM_CACHE_TYPE_PINNED | VCSM_CACHE_TYPE_HOST);
    case CacheType::GPU_CACHED:
        return static_cast<VCSM_CACHE_TYPE_T>(VCSM_CACHE_TYPE_PINNED | VCSM_CACHE_TYPE_VC);
    case CacheType::BOTH_CACHED:
    default:
        return static_cast<VCSM_CACHE_TYPE_T>(VCSM_CACHE_TYPE_PINNED | VCSM_CACHE_TYPE_HOST_AND_VC);
    }
}

std::unique_ptr<DeviceBuffer> VCSM::allocateBuffer(
    const std::shared_ptr<SystemAccess>& system, unsigned sizeInBytes, const std::string& name, CacheType cacheType)
{
    auto handle = vcsm_malloc_cache(sizeInBytes, toCacheType(cacheType), name.data());
    if(handle == 0)
    {
        DEBUG_LOG(DebugLevel::SYSCALL,
            std::cout << "[VC4CL] Failed to allocate VCSM handle for " << sizeInBytes << " of data!" << std::endl)
        return nullptr;
    }

    auto hostPointer = vcsm_lock(handle);
    if(hostPointer == nullptr)
    {
        DEBUG_LOG(DebugLevel::SYSCALL, std::cout << "[VC4CL] Failed to lock VCSM handle: " << handle << std::endl)
        vcsm_free(handle);
        return nullptr;
    }

    DevicePointer qpuPointer{vcsm_vc_addr_from_hdl(handle)};
    if(static_cast<uint32_t>(qpuPointer) == 0)
    {
        DEBUG_LOG(DebugLevel::SYSCALL,
            std::cout << "[VC4CL] Failed to get bus address for VCSM handle: " << handle << std::endl)
        vcsm_unlock_ptr_sp(hostPointer, true);
        vcsm_free(handle);
        return nullptr;
    }

    DEBUG_LOG(DebugLevel::SYSCALL,
        std::cout << "Allocated " << sizeInBytes << " bytes of buffer: handle " << handle << ", device address "
                  << std::hex << "0x" << qpuPointer << ", host address " << hostPointer << std::dec << std::endl)
    return std::unique_ptr<DeviceBuffer>{new DeviceBuffer(system, handle, qpuPointer, hostPointer, sizeInBytes)};
}

bool VCSM::deallocateBuffer(const DeviceBuffer* buffer)
{
    int status;
    if(buffer->hostPointer)
    {
        // no need to flush the cache, since we don't care about the contents anymore
        if((status = vcsm_unlock_ptr_sp(buffer->hostPointer, true)) != 0)
        {
            errno = -status;
            perror("[VC4CL] Error in vcsm_unlock_ptr_sp");
            return false;
        }
    }
    vcsm_free(buffer->memHandle);
    DEBUG_LOG(DebugLevel::SYSCALL,
        std::cout << "Deallocated " << buffer->size << " bytes of buffer: handle " << buffer->memHandle
                  << ", device address " << std::hex << "0x" << buffer->qpuPointer << ", host address "
                  << buffer->hostPointer << std::dec << std::endl)
    return true;
}

static std::unique_ptr<vcsm_user_clean_invalid2_s, decltype(&free)> allocateInvalidateCleanData(uint8_t numBlocks)
{
    auto bufferSize = sizeof(vcsm_user_clean_invalid2_s) +
        numBlocks * sizeof(vcsm_user_clean_invalid2_s::vcsm_user_clean_invalid2_block_s);
    std::unique_ptr<vcsm_user_clean_invalid2_s, decltype(&free)> data{
        reinterpret_cast<vcsm_user_clean_invalid2_s*>(malloc(bufferSize)), free};
    memset(data.get(), '\0', bufferSize);
    data->op_count = numBlocks;
    return data;
}

bool VCSM::flushCPUCache(const std::vector<const DeviceBuffer*>& buffers)
{
    auto data = allocateInvalidateCleanData(static_cast<uint8_t>(buffers.size()));
    for(uint8_t i = 0; i < data->op_count; ++i)
    {
        data->s[i].invalidate_mode = VC_SM_CACHE_OP_FLUSH;
        data->s[i].block_count = 1;
        data->s[i].start_address = buffers[i]->hostPointer;
        data->s[i].block_size = buffers[i]->size;
        data->s[i].inter_block_stride = 0;
    }
    return vcsm_clean_invalid2(data.get()) == 0;
}

uint32_t VCSM::getTotalCMAMemory()
{
    // the value does not change on a running Linux kernel AFAIK, so only need to query it once
    static const uint32_t cmaMemory = []() {
        std::ifstream fis("/proc/meminfo");
        std::string line;
        while(std::getline(fis, line))
        {
            if(line.find("CmaTotal") != std::string::npos)
            {
                // line looks like: CmaTotal:            xyz kB
                auto start = line.find_first_of("0123456789");
                if(start != std::string::npos)
                    return static_cast<uint32_t>(strtoul(line.data() + start, nullptr, 0)) * 1024 /* kB -> B*/;
                // we found the line but failed to parse it
                break;
            }
        }
        return uint32_t{0};
    }();
    return cmaMemory;
}

bool VCSM::readValue(SystemQuery query, uint32_t& output) noexcept
{
    switch(query)
    {
    case SystemQuery::TOTAL_GPU_MEMORY_IN_BYTES:
        if(isUsingCMA())
        {
            output = getTotalCMAMemory();
            return true;
        }
        return false;
    default:
        return false;
    }
    return false;
}
