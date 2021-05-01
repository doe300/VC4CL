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

using namespace vc4cl;

#ifdef MOCK_HAL
static const bool isEmulated = true;
#else
static const bool isEmulated = std::getenv("VC4CL_EMULATOR");
#endif

// Default to register-poking via the V3D interface
static const bool executeViaMailbox = std::getenv("VC4CL_EXECUTE_MAILBOX");
static const bool executeViaV3D = !executeViaMailbox;
// Defaults to memory management via the mailbox
static const bool manageMemoryViaCMA = std::getenv("VC4CL_MEMORY_CMA");
static const bool manageMemoryViaVCSM = manageMemoryViaCMA || std::getenv("VC4CL_MEMORY_VCSM");
static const bool manageMemoryViaMailbox = !manageMemoryViaVCSM;

static const bool enableMailbox = (executeViaMailbox || manageMemoryViaMailbox) && !std::getenv("VC4CL_NO_MAILBOX");
static const bool enableV3D = (true || executeViaV3D) && !std::getenv("VC4CL_NO_V3D");
static const bool enableVCSM = (manageMemoryViaVCSM) && !std::getenv("VC4CL_NO_VCSM");

static const std::pair<bool, CacheType> forcedCacheType = []() {
    auto envvar = std::getenv("VC4CL_CACHE_FORCE");
    if(!envvar)
        return std::make_pair(false, CacheType::UNCACHED);
    std::string env(envvar);
    auto start = env.find_first_of("0123456789");
    if(start != std::string::npos)
        return std::make_pair(true, static_cast<CacheType>(strtoul(env.data() + start, nullptr, 0)));
    return std::make_pair(false, CacheType::UNCACHED);
}();

static std::unique_ptr<Mailbox> initializeMailbox()
{
    if(isEmulated || !enableMailbox)
        return nullptr;
    return std::unique_ptr<Mailbox>(new Mailbox());
}

static std::unique_ptr<V3D> initializeV3D()
{
    if(isEmulated || !enableV3D)
        return nullptr;
    return std::unique_ptr<V3D>(new V3D());
}

static std::unique_ptr<VCSM> initializeVCSM()
{
    if(isEmulated || !manageMemoryViaVCSM || !enableVCSM)
        return nullptr;
    return std::unique_ptr<VCSM>{new VCSM(manageMemoryViaCMA)};
}

SystemAccess::SystemAccess() : mailbox(initializeMailbox()), v3d(initializeV3D()), vcsm(initializeVCSM())
{
    if(isEmulated)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS, std::cout << "[VC4CL] Using emulated system accesses " << std::endl)
    if(mailbox)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using mailbox for: " << (executeViaMailbox ? "kernel execution, " : "")
                      << (manageMemoryViaMailbox ? "memory allocation, " : "") << "system queries" << std::endl)
    if(v3d)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using V3D for: " << (executeViaV3D ? "kernel execution, " : "")
                      << "profiling, system queries" << std::endl)
    if(vcsm)
        DEBUG_LOG(DebugLevel::SYSTEM_ACCESS,
            std::cout << "[VC4CL] Using VCSM (" << (vcsm->isUsingCMA() ? "CMA" : "non-CMA")
                      << ") for: " << (manageMemoryViaVCSM ? "memory allocation" : "") << std::endl)
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
    return static_cast<uint8_t>(v3d->getSystemInfo(SystemInfo::QPU_COUNT));
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
    return 0;
}

std::unique_ptr<DeviceBuffer> SystemAccess::allocateBuffer(
    unsigned sizeInBytes, const std::string& name, CacheType cacheType)
{
    auto effectiveCacheType = forcedCacheType.first ? forcedCacheType.second : cacheType;
    if(vcsm)
        return vcsm->allocateBuffer(shared_from_this(), sizeInBytes, name, effectiveCacheType);
    if(mailbox)
        return mailbox->allocateBuffer(shared_from_this(), sizeInBytes, effectiveCacheType);
    return nullptr;
}

bool SystemAccess::deallocateBuffer(const DeviceBuffer* buffer)
{
    if(vcsm)
        return vcsm->deallocateBuffer(buffer);
    if(mailbox)
        return mailbox->deallocateBuffer(buffer);
    return false;
}

ExecutionHandle SystemAccess::executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> controlAddress,
    bool flushBuffer, std::chrono::milliseconds timeout)
{
    if(isEmulated)
        return ExecutionHandle(emulateQPU(numQPUs, controlAddress.second, timeout));
    if(executeViaMailbox)
    {
        if(!mailbox)
            return ExecutionHandle{false};
        return mailbox->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    }
    if(v3d)
        return v3d->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    return ExecutionHandle{false};
}

bool SystemAccess::executesKernelsViaV3D() const
{
    return v3d && executeViaV3D;
}

std::shared_ptr<SystemAccess>& vc4cl::system()
{
    static std::shared_ptr<SystemAccess> sys{new SystemAccess()};
    return sys;
}
