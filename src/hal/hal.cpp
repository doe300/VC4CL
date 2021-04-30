/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "hal.h"

#include "Mailbox.h"
#include "V3D.h"
#include "emulator.h"

#include <cstdlib>
#include <thread>

using namespace vc4cl;

#ifdef MOCK_HAL
static const bool isEmulated = true;
#else
static const bool isEmulated = std::getenv("VC4CL_EMULATOR");
#endif

// Default to register-poking via the V3D interface
static const bool executeViaMailbox = std::getenv("VC4CL_EXECUTE_MAILBOX");

SystemAccess::SystemAccess() : mailbox(new Mailbox()), v3d(new V3D())
{
    // TODO log the selected access methods
}

uint32_t SystemAccess::getTotalGPUMemory()
{
    if(isEmulated)
        return getTotalEmulatedMemory();
    return mailbox->getTotalGPUMemory();
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
    QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> msg({static_cast<uint32_t>(VC4Clock::V3D)});
    return mailbox->readMailboxMessage(msg) ? msg.getContent(1) : 0;
}

uint32_t SystemAccess::getGPUTemperatureInMilliDegree()
{
    if(isEmulated)
        return getEmulatedGPUTemperatureInMilliDegree();
    QueryMessage<MailboxTag::GET_TEMPERATURE> msg({0});
    //"Return the temperature of the SoC in thousandths of a degree C. id should be zero."
    return mailbox->readMailboxMessage(msg) ? msg.getContent(1) : 0;
}

uint32_t SystemAccess::getTotalVPMMemory()
{
    if(isEmulated)
        return getTotalEmulatedVPMMemory();
    return v3d->getSystemInfo(SystemInfo::VPM_MEMORY_SIZE);
}

std::unique_ptr<DeviceBuffer> SystemAccess::allocateBuffer(unsigned sizeInBytes, unsigned alignmentInBytes)
{
    return mailbox->allocateBuffer(shared_from_this(), sizeInBytes, alignmentInBytes);
}

bool SystemAccess::deallocateBuffer(const DeviceBuffer* buffer)
{
    return mailbox->deallocateBuffer(buffer);
}

ExecutionHandle SystemAccess::executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> controlAddress,
    bool flushBuffer, std::chrono::milliseconds timeout)
{
    if(isEmulated)
        return ExecutionHandle(emulateQPU(numQPUs, controlAddress.second, timeout));
    if(executeViaMailbox)
    {
        if(!flushBuffer)
            /*
             * For some reason, successive mailbox-calls without delays freeze the system (does the kernel get too
             * swamped??) A delay of 1ms has the same effect as no delay, 10ms slow down the execution, but work
             *
             * clpeak's global-bandwidth test runs ok without delay
             * clpeaks's compute-sp test hangs/freezes with/without delay
             */
            // TODO test with less delay? hangs? works? better performance?
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return mailbox->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
    }
    return v3d->executeQPU(numQPUs, controlAddress, flushBuffer, timeout);
}

bool SystemAccess::executesKernelsViaV3D() const
{
    return v3d && !executeViaMailbox;
}

std::shared_ptr<SystemAccess>& vc4cl::system()
{
    static std::shared_ptr<SystemAccess> sys{new SystemAccess()};
    return sys;
}
