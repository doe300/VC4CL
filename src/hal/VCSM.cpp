/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "VCSM.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

using namespace vc4cl;

#ifdef __cplusplus
extern "C"
{
#endif

    // see "/opt/vc/include/interface/vcsm/user-vcsm.h"
    typedef enum
    {
        VCSM_CACHE_TYPE_NONE = 0,    // No caching applies.
        VCSM_CACHE_TYPE_HOST,        // Allocation is cached on host (user space).
        VCSM_CACHE_TYPE_VC,          // Allocation is cached on videocore.
        VCSM_CACHE_TYPE_HOST_AND_VC, // Allocation is cached on both host and videocore.

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

#ifdef __cplusplus
}
#endif

VCSM::VCSM(bool useCMA) : usesCMA(useCMA)
{
    if(vcsm_init_ex(useCMA, -1) != 0)
        throw std::runtime_error("Failed to initialize VCSM!");

    // TODO to be precise, would need to check whether we actually use CMA, since the library will fall back to the
    // other interface if needed. This can be done by e.g. vcsm_export_dmabuf
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
        return VCSM_CACHE_TYPE_NONE;
    case CacheType::HOST_CACHED:
        return VCSM_CACHE_TYPE_HOST;
    case CacheType::GPU_CACHED:
        return VCSM_CACHE_TYPE_VC;
    case CacheType::BOTH_CACHED:
    default:
        return VCSM_CACHE_TYPE_HOST_AND_VC;
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

    DevicePointer qpuPointer{vcsm_vc_addr_from_hdl(handle)};
    if(static_cast<uint32_t>(qpuPointer) == 0)
    {
        DEBUG_LOG(DebugLevel::SYSCALL,
            std::cout << "[VC4CL] Failed to get bus address for VCSM handle: " << handle << std::endl)
        vcsm_free(handle);
        return nullptr;
    }

    auto hostPointer = vcsm_lock(handle);
    if(hostPointer == nullptr)
    {
        DEBUG_LOG(DebugLevel::SYSCALL, std::cout << "[VC4CL] Failed to lock VCSM handle: " << handle << std::endl)
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

bool VCSM::flushCPUCache(const DeviceBuffer* buffer)
{
    auto data = allocateInvalidateCleanData(1);
    data->s[0].invalidate_mode = 2; // clean
    data->s[0].block_count = 1;
    data->s[0].start_address = buffer->hostPointer;
    data->s[0].block_size = buffer->size;
    data->s[0].inter_block_stride = 0;
    return vcsm_clean_invalid2(data.get()) == 0;
}

bool VCSM::clearCPUCache(const DeviceBuffer* buffer)
{
    auto data = allocateInvalidateCleanData(1);
    data->s[0].invalidate_mode = 1; // invalidate
    data->s[0].block_count = 1;
    data->s[0].start_address = buffer->hostPointer;
    data->s[0].block_size = buffer->size;
    data->s[0].inter_block_stride = 0;
    return vcsm_clean_invalid2(data.get()) == 0;
}

uint32_t VCSM::getTotalGPUMemory() const
{
    if(usesCMA)
    {
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
        return 0;
    }
    // TODO need to retrieve either via mailbox or the interface used by vcgencmd
    throw std::runtime_error{"Cannot query non-CMA GPU memory via VCSM!"};
}
