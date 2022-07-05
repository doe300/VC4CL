/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "hal.h"

#include "Mailbox.h"
#include "V3D.h"
#include "VCHI.h"
#ifndef NO_VCSM
#include "VCSM.h"
#endif
#include "emulator.h"
#include "userland.h"

#include <cstdlib>
#include <unistd.h>

using namespace vc4cl;

static bool getEmulated()
{
#ifdef MOCK_HAL
    return true;
#else
    return std::getenv("VC4CL_EMULATOR");
#endif
}

static bool isRoot()
{
    // we might not be "root" user, but at least have elevated effective privileges
    return geteuid() == 0;
}

static ExecutionMode getExecMode()
{
    if(std::getenv("VC4CL_EXECUTE_REGISTER_POKING"))
        return ExecutionMode::V3D_REGISTER_POKING;
    if(std::getenv("VC4CL_EXECUTE_MAILBOX"))
        return ExecutionMode::MAILBOX_IOCTL;
    if(std::getenv("VC4CL_EXECUTE_VCHI"))
        return ExecutionMode::VCHI_GPU_SERVICE;

    // mailbox and VCHI GPU service execution do not require root rights
    // Due to https://github.com/raspberrypi/linux/issues/4321, do not use mailbox as default for anything
    // keep V3D as default for root for now to keep current behavior
    return isRoot() ? ExecutionMode::V3D_REGISTER_POKING : ExecutionMode::VCHI_GPU_SERVICE;
}

static MemoryManagement getMemoryMode()
{
    #ifndef NO_VCSM
    if(std::getenv("VC4CL_MEMORY_CMA"))
        return MemoryManagement::VCSM_CMA;
    if(std::getenv("VC4CL_MEMORY_VCSM"))
        return MemoryManagement::VCSM;
    #endif
    if(std::getenv("VC4CL_MEMORY_MAILBOX"))
        return MemoryManagement::MAILBOX;

    // fall back to VCSM CMA memory management, since it does not require root rights and select CMA due to:
    // - "old" VCSM is no longer supported beginning kernel 5.9 (or 5.10), see
    //   https://github.com/raspberrypi/linux/issues/4112
    // - CMA is more interoperable with other GPU memory management (e.g. mesa) and can be monitored via debugfs
    //  (/sys/kernel/debug/dma_buf/bufinfo and /sys/kernel/debug/vcsm-cma/state)
    // for root keep the current default of mailbox memory management, since this will already be configured correctly
    #ifndef NO_VCSM
    return isRoot() ? MemoryManagement::MAILBOX : MemoryManagement::VCSM_CMA;
    #else
    return MemoryManagement::MAILBOX;
    #endif
}

static std::pair<bool, CacheType> getForcedCacheType()
{
    auto envvar = std::getenv("VC4CL_CACHE_FORCE");
    if(!envvar)
        return std::make_pair(false, CacheType::UNCACHED);
    std::string env(envvar);
    auto start = env.find_first_of("0123456789");
    if(start != std::string::npos)
        return std::make_pair(true, static_cast<CacheType>(strtoul(env.data() + start, nullptr, 0)));
    return std::make_pair(false, CacheType::UNCACHED);
}

static std::unique_ptr<Mailbox> initializeMailbox(bool isEmulated, ExecutionMode execMode, MemoryManagement memoryMode)
{
    if(isEmulated || std::getenv("VC4CL_NO_MAILBOX"))
        // explicitly disabled
        return nullptr;
    if(execMode != ExecutionMode::MAILBOX_IOCTL && memoryMode != MemoryManagement::MAILBOX)
        // no need for the component
        return nullptr;
    return std::unique_ptr<Mailbox>(new Mailbox());
}

static std::unique_ptr<V3D> initializeV3D(bool isEmulated, ExecutionMode execMode)
{
    if(isEmulated || std::getenv("VC4CL_NO_V3D"))
        // explicitly disabled
        return nullptr;

    if(isDebugModeEnabled(DebugLevel::PERFORMANCE_COUNTERS) || execMode == ExecutionMode::V3D_REGISTER_POKING ||
        isRoot() /* we are root (or at least have root rights), so we can access the registers */)
        return std::unique_ptr<V3D>(new V3D());

    // by default, do not activate, since it requires root access
    return nullptr;
}

#ifndef NO_VCSM
static std::unique_ptr<VCSM> initializeVCSM(bool isEmulated, MemoryManagement memoryMode)
{
    if(isEmulated || std::getenv("VC4CL_NO_VCSM"))
        // explicitly disabled
        return nullptr;
    if(memoryMode != MemoryManagement::VCSM && memoryMode != MemoryManagement::VCSM_CMA)
        // no need for the component
        return nullptr;
    return std::unique_ptr<VCSM>{new VCSM(memoryMode == MemoryManagement::VCSM_CMA)};
}
#endif

static std::unique_ptr<VCHI> initializeVCHI(bool isEmulated, ExecutionMode execMode)
{
    if(isEmulated || std::getenv("VC4CL_NO_VCHI"))
        // explicitly disabled
        return nullptr;
    if(execMode != ExecutionMode::VCHI_GPU_SERVICE)
        // no need for the component
        return nullptr;
    return std::unique_ptr<VCHI>{new VCHI()};
}

SystemAccess::SystemAccess() :
    isEmulated(getEmulated()), 
    executionMode(getExecMode()), 
    memoryManagement(getMemoryMode()),	
    forcedCacheType(getForcedCacheType()), 
    mailbox(initializeMailbox(isEmulated, executionMode, memoryManagement)),
    v3d(initializeV3D(isEmulated, executionMode)), 
    #ifndef NO_VCSM
    vcsm(initializeVCSM(isEmulated, memoryManagement)),
    #endif
    vchi(initializeVCHI(isEmulated, executionMode))
{
    if(isEmulated)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS, std::cout << "[VC4CL] Using emulated system accesses " << std::endl)
    if(mailbox)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using mailbox for: "
                      << (executionMode == ExecutionMode::MAILBOX_IOCTL ? "kernel execution, " : "")
                      << (memoryManagement == MemoryManagement::MAILBOX ? "memory allocation, " : "")
                      << "system queries" << std::endl)
    if(v3d)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using V3D for: "
                      << (executionMode == ExecutionMode::V3D_REGISTER_POKING ? "kernel execution, " : "")
                      << "profiling, system queries" << std::endl)
    #ifndef NO_VCSM		
    if(vcsm)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using VCSM (" << (vcsm->isUsingCMA() ? "CMA" : "non-CMA") << ") for: "
                      << (memoryManagement == MemoryManagement::VCSM || memoryManagement == MemoryManagement::VCSM_CMA ?
                                 "memory allocation" :
                                 "")
                      << std::endl)
    #endif
    if(vchi)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using VCHI for: "
                      << (executionMode == ExecutionMode::VCHI_GPU_SERVICE ? "kernel execution" : "") << std::endl)

    if(forcedCacheType.first)
    {
        std::string cacheType;
        switch(forcedCacheType.second)
        {
        case CacheType::UNCACHED:
            cacheType = "uncached";
            break;
        case CacheType::HOST_CACHED:
            cacheType = "host cached";
            break;
        case CacheType::GPU_CACHED:
            cacheType = "GPU cached";
            break;
        case CacheType::BOTH_CACHED:
            cacheType = "host and GPU cached";
            break;
        }
        DEBUG_LOG(
            DebugLevel::SYSTEM_ACCESS, std::cout << "[VC4CL] Forcing memory caching type: " << cacheType << std::endl)
    }
}

uint32_t SystemAccess::getTotalVPMMemory()
{
    /*
     * Assume all accessible VPM memory:
     * Due to a hardware bug (HW-2253), user programs can only use the first 64 rows of VPM, resulting in a total of 4KB
     * available VPM cache size (64 * 16 * sizeof(uint))
     */
    return querySystem(SystemQuery::TOTAL_VPM_MEMORY_IN_BYTES, 64 * 16 * sizeof(uint32_t));
}

uint32_t SystemAccess::querySystem(SystemQuery query, uint32_t defaultValue)
{
    uint32_t value = defaultValue;
    if(isEmulated)
        return getEmulatedSystemQuery(query);
    #ifndef NO_VCSM
    if(vcsm && vcsm->readValue(query, value))
        return value;
    #endif
    if(v3d && v3d->readValue(query, value))
        return value;
    if(vchi && vchi->readValue(query, value))
        return value;
    if(mailbox && mailbox->readValue(query, value))
        return value;
    return defaultValue;
}

std::string SystemAccess::getModelType()
{
    if(isEmulated)
        return "(emulated)";

    switch(bcm_host_get_model_type())
    {
    case BCM_HOST_BOARD_TYPE_MODELA:
        return "A";
    case BCM_HOST_BOARD_TYPE_MODELB:
        return "B";
    case BCM_HOST_BOARD_TYPE_MODELAPLUS:
        return "A+";
    case BCM_HOST_BOARD_TYPE_MODELBPLUS:
        return "B+";
    case BCM_HOST_BOARD_TYPE_PI2MODELB:
        return "2 B";
    case BCM_HOST_BOARD_TYPE_CM:
        return "CM";
    case BCM_HOST_BOARD_TYPE_CM2:
        return "2 CM";
    case BCM_HOST_BOARD_TYPE_PI3MODELB:
        return "3 B+";
    case BCM_HOST_BOARD_TYPE_PI0:
        return "Zero";
    case BCM_HOST_BOARD_TYPE_CM3:
        return "3 CM";
    case BCM_HOST_BOARD_TYPE_PI0W:
        return "Zero W";
    case BCM_HOST_BOARD_TYPE_PI3MODELBPLUS:
        return "3 B+";
    case BCM_HOST_BOARD_TYPE_PI3MODELAPLUS:
        return "3 A+";
    case BCM_HOST_BOARD_TYPE_CM3PLUS:
        return "3 CM+";
    case BCM_HOST_BOARD_TYPE_PI4MODELB:
        return "4 B";
    case BCM_HOST_BOARD_TYPE_PI400:
        return "400";
    case BCM_HOST_BOARD_TYPE_CM4:
        return "4 CM";
    default:
        return "unknown";
    }
}

std::string SystemAccess::getProcessorType()
{
    if(isEmulated)
        return "(emulated)";

    switch(bcm_host_get_processor_id())
    {
    case BCM_HOST_PROCESSOR_BCM2835:
        return "BCM2835";
    case BCM_HOST_PROCESSOR_BCM2836:
        return "BCM2836";
    case BCM_HOST_PROCESSOR_BCM2837:
        return "BCM2837";
    case BCM_HOST_PROCESSOR_BCM2838:
        return "BCM2838";
    default:
        return "unknown";
    }
}

std::unique_ptr<DeviceBuffer> SystemAccess::allocateBuffer(
    unsigned sizeInBytes, const std::string& name, CacheType cacheType)
{
    if(isEmulated)
        return allocateEmulatorBuffer(shared_from_this(), sizeInBytes);
    auto effectiveCacheType = forcedCacheType.first ? forcedCacheType.second : cacheType;
    #ifndef NO_VCSM
    if(vcsm && (memoryManagement == MemoryManagement::VCSM || memoryManagement == MemoryManagement::VCSM_CMA))
        return vcsm->allocateBuffer(shared_from_this(), sizeInBytes, name, effectiveCacheType);
    #endif
    if(mailbox && memoryManagement == MemoryManagement::MAILBOX)
        return mailbox->allocateBuffer(shared_from_this(), sizeInBytes, effectiveCacheType);
    return nullptr;
}

std::unique_ptr<DeviceBuffer> SystemAccess::allocateGPUOnlyBuffer(
    unsigned sizeInBytes, const std::string& name, CacheType cacheType)
{
    return allocateBuffer(sizeInBytes, name, cacheType);
}

bool SystemAccess::deallocateBuffer(const DeviceBuffer* buffer)
{
    if(isEmulated)
        deallocateEmulatorBuffer(buffer);
    #ifndef NO_VCSM	
    if(vcsm && (memoryManagement == MemoryManagement::VCSM || memoryManagement == MemoryManagement::VCSM_CMA))
        return vcsm->deallocateBuffer(buffer);
    #endif
    if(mailbox && memoryManagement == MemoryManagement::MAILBOX)
        return mailbox->deallocateBuffer(buffer);
    return false;
}

bool SystemAccess::flushCPUCache(const std::vector<const DeviceBuffer*>& buffers)
{
    #ifndef NO_VCSM
    if(vcsm && (memoryManagement == MemoryManagement::VCSM || memoryManagement == MemoryManagement::VCSM_CMA))
        return vcsm->flushCPUCache(buffers);
    #endif
    return false;
}

ExecutionHandle SystemAccess::executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> controlAddress,
    bool flushBuffer, std::chrono::milliseconds timeout)
{
    if(isEmulated)
        return ExecutionHandle(emulateQPU(numQPUs, controlAddress.second, timeout));
    if(vchi && executionMode == ExecutionMode::VCHI_GPU_SERVICE)
        return vchi->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    if(mailbox && executionMode == ExecutionMode::MAILBOX_IOCTL)
        return mailbox->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    if(v3d && executionMode == ExecutionMode::V3D_REGISTER_POKING)
        return v3d->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    return ExecutionHandle{false};
}

std::shared_ptr<SystemAccess>& vc4cl::system()
{
    static std::shared_ptr<SystemAccess> sys{new SystemAccess()};
    return sys;
}
