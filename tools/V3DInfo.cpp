/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "common.h"
#include "hal/Mailbox.h"

using namespace vc4cl;

static constexpr size_t NAME_LENGTH = 24;
static constexpr size_t VAL_LENGTH = 16;

static void checkResult(bool result)
{
    if(!result)
        throw std::runtime_error("Error in mailbox-call!");
}

static void printModelInfo(SystemAccess& system)
{
    std::cout << std::setw(NAME_LENGTH) << "Model:" << std::setw(VAL_LENGTH) << system.getModelType() << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Processor:" << std::setw(VAL_LENGTH) << system.getProcessorType()
              << std::endl;

    uint32_t revision = 0;
    if(auto mb = system.getMailboxIfAvailable())
    {
        SimpleQueryMessage<MailboxTag::BOARD_REVISION> msg;
        checkResult(mb->readMailboxMessage(msg));
        revision = msg.getContent(0);
    }

    if(revision == 0)
    {
        std::cout << std::setw(NAME_LENGTH) << "Warranty:" << std::setw(VAL_LENGTH) << "failed to detect" << std::endl;
        return;
    }

    // This below information is  taken from https://github.com/AndrewFromMelbourne/raspberry_pi_revision
    if(revision & 0x800000)
        // Raspberry Pi2 style revision encoding
        std::cout << std::setw(NAME_LENGTH) << "Warranty void:" << std::setw(VAL_LENGTH)
                  << (revision & 0x2000000 ? "yes" : "no") << std::endl;
    else
        std::cout << std::setw(NAME_LENGTH) << "Warranty void:" << std::setw(VAL_LENGTH)
                  << (revision & 0x1000000 ? "yes" : "no") << std::endl;
}

static std::string toString(ExecutionMode mode)
{
    switch(mode)
    {
    case ExecutionMode::MAILBOX_IOCTL:
        return "mailbox";
    case ExecutionMode::V3D_REGISTER_POKING:
        return "V3D register poking";
    case ExecutionMode::VCHI_GPU_SERVICE:
        return "VCHI GPU service";
    }
    throw std::invalid_argument{"Unknown execution mode: " + std::to_string(static_cast<unsigned>(mode))};
}

static std::string toString(MemoryManagement mode)
{
    switch(mode)
    {
    case MemoryManagement::MAILBOX:
        return "mailbox";
    #ifndef NO_VCSM
    case MemoryManagement::VCSM:
        return "VCSM";
    case MemoryManagement::VCSM_CMA:
        return "VCSM CMA";
    #endif
    }
    throw std::invalid_argument{"Unknown memory management mode: " + std::to_string(static_cast<unsigned>(mode))};
}

static void printVC4CLInfo()
{
    std::cout << "VC4CL info:" << std::endl;

    std::cout << std::setw(NAME_LENGTH) << "Version:" << std::setw(VAL_LENGTH) << platform_config::VC4CL_VERSION
              << std::endl;

#ifdef HAS_COMPILER
    std::cout << std::setw(NAME_LENGTH) << "Compiler:" << std::setw(VAL_LENGTH) << "available" << std::endl;
#else
    std::cout << std::setw(NAME_LENGTH) << "Compiler:" << std::setw(VAL_LENGTH) << "unavailable" << std::endl;
#endif

#if defined(cl_khr_icd) and defined(use_cl_khr_icd)
    std::cout << std::setw(NAME_LENGTH) << "ICD loader support:" << std::setw(VAL_LENGTH) << "enabled" << std::endl;
#else
    std::cout << std::setw(NAME_LENGTH) << "ICD loader support:" << std::setw(VAL_LENGTH) << "disabled" << std::endl;
#endif

#ifdef IMAGE_SUPPORT
    std::cout << std::setw(NAME_LENGTH) << "Image support:" << std::setw(VAL_LENGTH) << "enabled" << std::endl;
#else
    std::cout << std::setw(NAME_LENGTH) << "Image support:" << std::setw(VAL_LENGTH) << "disabled" << std::endl;
#endif

    std::cout << std::setw(NAME_LENGTH) << "Execution mode:" << std::setw(VAL_LENGTH)
              << toString(system()->executionMode) << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Memory management mode:" << std::setw(VAL_LENGTH)
              << toString(system()->memoryManagement) << std::endl;
}

static void printSystemOverview(SystemAccess& system)
{
    std::cout << "System Overview:" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "ARM Memory:" << std::setw(VAL_LENGTH)
              << (system.querySystem(SystemQuery::TOTAL_ARM_MEMORY_IN_BYTES, 0) / 1024 / 1024) << " MB" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "VideoCore IV Memory:" << std::setw(VAL_LENGTH)
              << (system.querySystem(SystemQuery::TOTAL_GPU_MEMORY_IN_BYTES, 0) / 1024 / 1024) << " MB" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Clock Rate (ARM):" << std::setw(VAL_LENGTH)
              << (system.querySystem(SystemQuery::CURRENT_ARM_CLOCK_RATE_IN_HZ, 0) / 1000000) << " MHz (max. "
              << (system.querySystem(SystemQuery::MAXIMUM_ARM_CLOCK_RATE_IN_HZ, 0) / 1000000) << " MHz)" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Clock Rate (V3D):" << std::setw(VAL_LENGTH)
              << (system.querySystem(SystemQuery::CURRENT_QPU_CLOCK_RATE_IN_HZ, 0) / 1000000) << " MHz (max. "
              << (system.querySystem(SystemQuery::MAXIMUM_QPU_CLOCK_RATE_IN_HZ, 0) / 1000000) << " MHz)" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "SoC Temperature:" << std::setw(VAL_LENGTH)
              << (system.querySystem(SystemQuery::QPU_TEMPERATURE_IN_MILLI_DEGREES, 0) / 1000) << " C" << std::endl;
}

static void printMailboxInfo(Mailbox& mb)
{
    std::cout << "Mailbox Info:" << std::endl;
    {
        SimpleQueryMessage<MailboxTag::FIRMWARE_REVISION> msg;
        checkResult(mb.readMailboxMessage(msg));
        std::cout << std::setw(NAME_LENGTH) << "Firmware Revision:" << std::setw(VAL_LENGTH) << std::hex
                  << msg.getContent(0) << std::dec << std::endl;
    }
    {
        SimpleQueryMessage<MailboxTag::BOARD_MODEL> msg;
        checkResult(mb.readMailboxMessage(msg));
        std::cout << std::setw(NAME_LENGTH) << "Board Model:" << std::setw(VAL_LENGTH) << msg.getContent(0)
                  << std::endl;
    }
    {
        SimpleQueryMessage<MailboxTag::BOARD_REVISION> msg;
        checkResult(mb.readMailboxMessage(msg));
        std::cout << std::setw(NAME_LENGTH) << "Board Revision:" << std::setw(VAL_LENGTH) << std::hex
                  << msg.getContent(0) << std::dec << std::endl;
    }
    {
        SimpleQueryMessage<MailboxTag::MAC_ADDRESS> msg;
        checkResult(mb.readMailboxMessage(msg));
        // XXX 6 byte MAC
        std::cout << std::setw(NAME_LENGTH) << "MAC Address:" << std::setw(VAL_LENGTH) << std::hex << msg.getContent(0)
                  << std::dec << std::endl;
    }
    {
        SimpleQueryMessage<MailboxTag::BOARD_SERIAL> msg;
        checkResult(mb.readMailboxMessage(msg));
        std::cout << std::setw(NAME_LENGTH) << "Board Serial:" << std::setw(VAL_LENGTH) << std::hex << msg.getContent(0)
                  << " " << msg.getContent(1) << std::dec << std::endl;
    }
    {
        SimpleQueryMessage<MailboxTag::ARM_MEMORY> msg;
        checkResult(mb.readMailboxMessage(msg));
        std::cout << std::setw(NAME_LENGTH) << "ARM Memory:" << std::setw(VAL_LENGTH) << msg.getContent(1) << " Bytes ("
                  << (msg.getContent(1) / 1024 / 1024) << " MB)" << std::endl;
    }
    {
        SimpleQueryMessage<MailboxTag::VC_MEMORY> msg;
        checkResult(mb.readMailboxMessage(msg));
        std::cout << std::setw(NAME_LENGTH) << "VideoCore IV Memory:" << std::setw(VAL_LENGTH) << msg.getContent(1)
                  << " Bytes (" << (msg.getContent(1) / 1024 / 1024) << " MB)" << std::endl;
    }
    {
        QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::ARM)});
        QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::ARM)});
        QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::ARM)});
        checkResult(mb.readMailboxMessage(rateMsg));
        checkResult(mb.readMailboxMessage(maxMsg));
        checkResult(mb.readMailboxMessage(minMsg));
        std::cout << std::setw(NAME_LENGTH) << "Clock Rate (ARM):" << std::setw(VAL_LENGTH)
                  << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to "
                  << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
    }
    {
        QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::CORE)});
        QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::CORE)});
        QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::CORE)});
        checkResult(mb.readMailboxMessage(rateMsg));
        checkResult(mb.readMailboxMessage(maxMsg));
        checkResult(mb.readMailboxMessage(minMsg));
        std::cout << std::setw(NAME_LENGTH) << "Clock Rate (Core):" << std::setw(VAL_LENGTH)
                  << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to "
                  << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
    }
    {
        QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::V3D)});
        QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::V3D)});
        QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::V3D)});
        checkResult(mb.readMailboxMessage(rateMsg));
        checkResult(mb.readMailboxMessage(maxMsg));
        checkResult(mb.readMailboxMessage(minMsg));
        std::cout << std::setw(NAME_LENGTH) << "Clock Rate (V3D):" << std::setw(VAL_LENGTH)
                  << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to "
                  << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
    }
    {
        QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::PWM)});
        QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::PWM)});
        QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::PWM)});
        checkResult(mb.readMailboxMessage(rateMsg));
        checkResult(mb.readMailboxMessage(maxMsg));
        checkResult(mb.readMailboxMessage(minMsg));
        std::cout << std::setw(NAME_LENGTH) << "Clock Rate (PWM):" << std::setw(VAL_LENGTH)
                  << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to "
                  << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
    }
    {
        QueryMessage<MailboxTag::GET_TEMPERATURE> tempMsg({0});
        QueryMessage<MailboxTag::GET_MAX_TEMPERATURE> maxMsg({0});
        checkResult(mb.readMailboxMessage(tempMsg));
        checkResult(mb.readMailboxMessage(maxMsg));
        std::cout << std::setw(NAME_LENGTH) << "SoC Temperature:" << std::setw(VAL_LENGTH)
                  << tempMsg.getContent(1) / 1000 << " C (max " << maxMsg.getContent(1) / 1000 << " C)" << std::endl;
    }
}

static void printV3DInfo(V3D& v3d)
{
    std::cout << "V3D Status Register Info:" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "V3D revision:" << std::setw(VAL_LENGTH)
              << v3d.getSystemInfo(SystemInfo::V3D_REVISION) << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "HDR support:" << std::setw(VAL_LENGTH)
              << (v3d.getSystemInfo(SystemInfo::HDR_SUPPORT) ? "yes" : "no") << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Semaphores:" << std::setw(VAL_LENGTH)
              << v3d.getSystemInfo(SystemInfo::SEMAPHORES_COUNT) << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "QPUs:" << std::setw(VAL_LENGTH) << v3d.getSystemInfo(SystemInfo::QPU_COUNT)
              << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Slices:" << std::setw(VAL_LENGTH)
              << v3d.getSystemInfo(SystemInfo::SLICES_COUNT) << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "VPM Memory size:" << std::setw(VAL_LENGTH)
              << v3d.getSystemInfo(SystemInfo::VPM_MEMORY_SIZE) / 1024 << " KB" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "VPM User size:" << std::setw(VAL_LENGTH)
              << v3d.getSystemInfo(SystemInfo::VPM_USER_MEMORY_SIZE) / 1024 << " KB" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Program queue:" << std::setw(VAL_LENGTH)
              << ((std::to_string(v3d.getSystemInfo(SystemInfo::USER_REQUESTS_COUNT)) + "/") +
                     (std::to_string(v3d.getSystemInfo(SystemInfo::USER_PROGRAMS_COMPLETED_COUNT)) + "/") +
                     std::to_string(v3d.getSystemInfo(SystemInfo::PROGRAM_QUEUE_LENGTH)))
              << " requests/completed/in queue" << std::endl;
    std::cout << std::setw(NAME_LENGTH) << "Errors:" << std::setw(VAL_LENGTH) << getErrors(v3d) << std::endl;

    // TODO performance counters ??
}

static void printMaximumAllocation()
{
    uint32_t maxSize = system()->getTotalGPUMemory();

    std::cout << "Testing maximum single allocation size (GPU memory: " << (maxSize) / (1024 * 1024)
              << "MB):" << std::endl;
    bool couldAllocateAnything = false;
    for(uint32_t trySize = maxSize; trySize > 1; trySize >>= 1)
    {
        std::unique_ptr<DeviceBuffer> buffer(system()->allocateBuffer(trySize, "VC4CL V3D info"));
        if(buffer != nullptr)
        {
            std::cout << "Maximum single allocation: " << buffer->size << " bytes (" << (buffer->size / 1024) / 1024
                      << " MB)" << std::endl;
            couldAllocateAnything = true;
            break;
        }
    }
    if(!couldAllocateAnything)
        std::cout << "Failed to allocate any buffer!" << std::endl;
}

int main(int argc, char** argv)
{
    std::cout << "V3D Info:" << std::endl;
    std::cout << std::endl;

    printModelInfo(*system());
    printVC4CLInfo();
    if(auto mailbox = system()->getMailboxIfAvailable())
        printMailboxInfo(*mailbox);
    else
        printSystemOverview(*system());
    if(auto v3d = system()->getV3DIfAvailable())
        printV3DInfo(*v3d);
    printMaximumAllocation();
}
