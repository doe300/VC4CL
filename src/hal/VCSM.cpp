/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "VCSM.h"

#include "userland.h"

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <unistd.h>

using namespace vc4cl;

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
        vcsm_unlock_ptr(hostPointer);
        vcsm_free(handle);
        return nullptr;
    }

    DEBUG_LOG(DebugLevel::MEMORY,
        std::cout << "Allocated " << sizeInBytes << " bytes of buffer: handle " << handle << ", device address "
                  << std::hex << "0x" << qpuPointer << ", host address " << hostPointer << std::dec << std::endl)
    return std::unique_ptr<DeviceBuffer>{new DeviceBuffer(system, handle, qpuPointer, hostPointer, sizeInBytes)};
}

bool VCSM::deallocateBuffer(const DeviceBuffer* buffer)
{
    if(buffer->hostPointer)
    {
        if(int status = vcsm_unlock_ptr(buffer->hostPointer))
        {
            errno = -status;
            perror("[VC4CL] Error in vcsm_unlock_ptr");
            return false;
        }
    }
    vcsm_free(buffer->memHandle);
    DEBUG_LOG(DebugLevel::MEMORY,
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
