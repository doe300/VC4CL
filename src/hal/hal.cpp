/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "hal.h"

#include "Mailbox.h"
#include "V3D.h"
#include "VCSM.h"
#include "emulator.h"

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

static ExecutionMode getExecMode()
{
    if(std::getenv("VC4CL_EXECUTE_REGISTER_POKING"))
        return ExecutionMode::V3D_REGISTER_POKING;
    if(std::getenv("VC4CL_EXECUTE_MAILBOX"))
        return ExecutionMode::MAILBOX_IOCTL;

    // fall back to mailbox execution, since it does not require root rights
    return ExecutionMode::MAILBOX_IOCTL;
}

static MemoryManagement getMemoryMode()
{
    if(std::getenv("VC4CL_MEMORY_CMA"))
        return MemoryManagement::VCSM_CMA;
    if(std::getenv("VC4CL_MEMORY_VCSM"))
        return MemoryManagement::VCSM;
    if(std::getenv("VC4CL_MEMORY_MAILBOX"))
        return MemoryManagement::MAILBOX;

    // fall back to VCSM CMA memory management, since it does not require root rights and select CMA due to:
    // - "old" VCSM is no longer supported beginning kernel 5.9 (or 5.10), see
    //   https://github.com/raspberrypi/linux/issues/4112
    // - CMA is more interoperable with other GPU memory management (e.g. mesa) and can be monitored via debugfs
    //  (/sys/kernel/debug/dma_buf/bufinfo and /sys/kernel/debug/vcsm-cma/state)
    return MemoryManagement::VCSM_CMA;
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
    // TODO is mailbox required to boot up GPU/QPUS?
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
        geteuid() == 0 /* we are root (or at least have root rights), so we can access the registers */)
        return std::unique_ptr<V3D>(new V3D());

    // by default, do not activate, since it requires root access
    return nullptr;
}

static std::unique_ptr<VCSM> initializeVCSM(bool isEmulated, MemoryManagement memoryMode)
{
    if(isEmulated || std::getenv("VC4CL_NO_VCSM"))
        // explicitly disabled
        return nullptr;
    if(memoryMode == MemoryManagement::MAILBOX)
        // no need for the component
        return nullptr;
    return std::unique_ptr<VCSM>{new VCSM(memoryMode == MemoryManagement::VCSM_CMA)};
}

SystemAccess::SystemAccess() :
    isEmulated(getEmulated()), executionMode(getExecMode()), memoryManagement(getMemoryMode()),
    forcedCacheType(getForcedCacheType()), mailbox(initializeMailbox(isEmulated, executionMode, memoryManagement)),
    v3d(initializeV3D(isEmulated, executionMode)), vcsm(initializeVCSM(isEmulated, memoryManagement))
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
    if(vcsm)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using VCSM (" << (vcsm->isUsingCMA() ? "CMA" : "non-CMA") << ") for: "
                      << (memoryManagement == MemoryManagement::VCSM || memoryManagement == MemoryManagement::VCSM_CMA ?
                                 "memory allocation" :
                                 "")
                      << std::endl)
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

uint32_t SystemAccess::getTotalGPUMemory()
{
    if(isEmulated)
        return getTotalEmulatedMemory();
    if(vcsm && vcsm->isUsingCMA())
        return vcsm->getTotalGPUMemory();
    if(mailbox)
        return mailbox->getTotalGPUMemory();
    return 0;
}

uint8_t SystemAccess::getNumQPUs()
{
    if(isEmulated)
        return getNumEmulatedQPUs();
    if(v3d)
        return static_cast<uint8_t>(v3d->getSystemInfo(SystemInfo::QPU_COUNT));
    // all models have 12 QPUs
    return 12;
}

uint32_t SystemAccess::getQPUClockRateInHz()
{
    if(isEmulated)
        return getEmulatedQPUClockRateInHz();
    if(mailbox)
    {
        QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> msg({static_cast<uint32_t>(VC4Clock::V3D)});
        return mailbox->readMailboxMessage(msg) ? msg.getContent(1) : 0;
    }
    return 0;
}

uint32_t SystemAccess::getGPUTemperatureInMilliDegree()
{
    if(isEmulated)
        return getEmulatedGPUTemperatureInMilliDegree();
    if(mailbox)
    {
        QueryMessage<MailboxTag::GET_TEMPERATURE> msg({0});
        //"Return the temperature of the SoC in thousandths of a degree C. id should be zero."
        return mailbox->readMailboxMessage(msg) ? msg.getContent(1) : 0;
    }
    return 0;
}

uint32_t SystemAccess::getTotalVPMMemory()
{
    if(isEmulated)
        return getTotalEmulatedVPMMemory();
    if(v3d)
        return v3d->getSystemInfo(SystemInfo::VPM_MEMORY_SIZE);
    /*
     * Assume all accessible VPM memory:
     * Due to a hardware bug (HW-2253), user programs can only use the first 64 rows of VPM, resulting in a total of 4KB
     * available VPM cache size (64 * 16 * sizeof(uint))
     */
    return 64 * 16 * sizeof(uint32_t);
}

std::unique_ptr<DeviceBuffer> SystemAccess::allocateBuffer(
    unsigned sizeInBytes, const std::string& name, CacheType cacheType)
{
    if(isEmulated)
        return allocateEmulatorBuffer(shared_from_this(), sizeInBytes);
    auto effectiveCacheType = forcedCacheType.first ? forcedCacheType.second : cacheType;
    if(vcsm && (memoryManagement == MemoryManagement::VCSM || memoryManagement == MemoryManagement::VCSM_CMA))
        return vcsm->allocateBuffer(shared_from_this(), sizeInBytes, name, effectiveCacheType);
    if(mailbox && memoryManagement == MemoryManagement::MAILBOX)
        return mailbox->allocateBuffer(shared_from_this(), sizeInBytes, effectiveCacheType);
    return nullptr;
}

bool SystemAccess::deallocateBuffer(const DeviceBuffer* buffer)
{
    if(isEmulated)
        deallocateEmulatorBuffer(buffer);
    if(vcsm && (memoryManagement == MemoryManagement::VCSM || memoryManagement == MemoryManagement::VCSM_CMA))
        return vcsm->deallocateBuffer(buffer);
    if(mailbox && memoryManagement == MemoryManagement::MAILBOX)
        return mailbox->deallocateBuffer(buffer);
    return false;
}

ExecutionHandle SystemAccess::executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> controlAddress,
    bool flushBuffer, std::chrono::milliseconds timeout)
{
    if(isEmulated)
        return ExecutionHandle(emulateQPU(numQPUs, controlAddress.second, timeout));
    if(mailbox && executionMode == ExecutionMode::MAILBOX_IOCTL)
        return mailbox->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    if(v3d && executionMode == ExecutionMode::V3D_REGISTER_POKING)
        return v3d->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    return ExecutionHandle{false};
}

bool SystemAccess::executesKernelsViaV3D() const
{
    return v3d && executionMode == ExecutionMode::V3D_REGISTER_POKING;
}

std::shared_ptr<SystemAccess>& vc4cl::system()
{
    static std::shared_ptr<SystemAccess> sys{new SystemAccess()};
    return sys;
}
