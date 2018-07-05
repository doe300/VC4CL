/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <iostream>
#include <iomanip>

#include "Mailbox.h"
#include "common.h"

using namespace vc4cl;

static const size_t NAME_LENGTH = 24;
static const size_t VAL_LENGTH = 16;

static void checkResult(bool result)
{
	if(!result)
		throw std::runtime_error("Error in mailbox-call!");
}

static void printMailboxInfo()
{
	Mailbox& mb = mailbox();
	std::cout << "Mailbox Info:" << std::endl;
	{
		SimpleQueryMessage<MailboxTag::FIRMWARE_REVISION> msg;
		checkResult(mb.readMailboxMessage(msg));
		std::cout << std::setw(NAME_LENGTH) << "Firmware Revision:" << std::setw(VAL_LENGTH) << std::hex << msg.getContent(0) << std::dec << std::endl;
	}
	{
		SimpleQueryMessage<MailboxTag::BOARD_MODEL> msg;
		checkResult(mb.readMailboxMessage(msg));
		std::cout << std::setw(NAME_LENGTH) << "Board Model:" << std::setw(VAL_LENGTH) << msg.getContent(0) << std::endl;
	}
	{
		SimpleQueryMessage<MailboxTag::BOARD_REVISION> msg;
		checkResult(mb.readMailboxMessage(msg));
		std::cout << std::setw(NAME_LENGTH) << "Board Revision:" << std::setw(VAL_LENGTH) << std::hex << msg.getContent(0) << std::dec << std::endl;
	}
	{
		SimpleQueryMessage<MailboxTag::MAC_ADDRESS> msg;
		checkResult(mb.readMailboxMessage(msg));
		//XXX 6 byte MAC
		std::cout << std::setw(NAME_LENGTH) << "MAC Address:" << std::setw(VAL_LENGTH) << std::hex << msg.getContent(0) << std::dec << std::endl;
	}
	{
		SimpleQueryMessage<MailboxTag::BOARD_SERIAL> msg;
		checkResult(mb.readMailboxMessage(msg));
		std::cout << std::setw(NAME_LENGTH) << "Board Serial:" << std::setw(VAL_LENGTH) << std::hex << msg.getContent(0) << " " << msg.getContent(1) << std::dec << std::endl;
	}
	{
		SimpleQueryMessage<MailboxTag::ARM_MEMORY> msg;
		checkResult(mb.readMailboxMessage(msg));
		std::cout << std::setw(NAME_LENGTH) << "ARM Memory:" << std::setw(VAL_LENGTH) << msg.getContent(1) << " Bytes (" << (msg.getContent(1) / 1024 / 1024) << " MB)" << std::endl;
	}
	{
		SimpleQueryMessage<MailboxTag::VC_MEMORY> msg;
		checkResult(mb.readMailboxMessage(msg));
		std::cout << std::setw(NAME_LENGTH) << "VideoCore IV Memory:" << std::setw(VAL_LENGTH) << msg.getContent(1) << " Bytes (" << (msg.getContent(1) / 1024 / 1024) << " MB)" << std::endl;
	}
	{
		QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::ARM)});
		QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::ARM)});
		QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::ARM)});
		checkResult(mb.readMailboxMessage(rateMsg));
		checkResult(mb.readMailboxMessage(maxMsg));
		checkResult(mb.readMailboxMessage(minMsg));
		std::cout << std::setw(NAME_LENGTH) << "Clock Rate (ARM):" << std::setw(VAL_LENGTH) << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to " << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
	}
	{
		QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::CORE)});
		QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::CORE)});
		QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::CORE)});
		checkResult(mb.readMailboxMessage(rateMsg));
		checkResult(mb.readMailboxMessage(maxMsg));
		checkResult(mb.readMailboxMessage(minMsg));
		std::cout << std::setw(NAME_LENGTH) << "Clock Rate (Core):" << std::setw(VAL_LENGTH) << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to " << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
	}
	{
		QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::V3D)});
		QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::V3D)});
		QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::V3D)});
		checkResult(mb.readMailboxMessage(rateMsg));
		checkResult(mb.readMailboxMessage(maxMsg));
		checkResult(mb.readMailboxMessage(minMsg));
		std::cout << std::setw(NAME_LENGTH) << "Clock Rate (V3D):" << std::setw(VAL_LENGTH) << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to " << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
	}
	{
		QueryMessage<MailboxTag::GET_CLOCK_RATE> rateMsg({static_cast<unsigned>(VC4Clock::PWM)});
		QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> maxMsg({static_cast<unsigned>(VC4Clock::PWM)});
		QueryMessage<MailboxTag::GET_MIN_CLOCK_RATE> minMsg({static_cast<unsigned>(VC4Clock::PWM)});
		checkResult(mb.readMailboxMessage(rateMsg));
		checkResult(mb.readMailboxMessage(maxMsg));
		checkResult(mb.readMailboxMessage(minMsg));
		std::cout << std::setw(NAME_LENGTH) << "Clock Rate (PWM):" << std::setw(VAL_LENGTH) << rateMsg.getContent(1) / 1000000 << " MHz (" << minMsg.getContent(1) / 1000000 << " to " << maxMsg.getContent(1) / 1000000 << " MHz)" << std::endl;
	}
	{
		QueryMessage<MailboxTag::GET_TEMPERATURE> tempMsg({0});
		QueryMessage<MailboxTag::GET_MAX_TEMPERATURE> maxMsg({0});
		checkResult(mb.readMailboxMessage(tempMsg));
		checkResult(mb.readMailboxMessage(maxMsg));
		std::cout << std::setw(NAME_LENGTH) << "SoC Temperature:" << std::setw(VAL_LENGTH) << tempMsg.getContent(1) / 1000 << " C (max " << maxMsg.getContent(1) / 1000 << " C)"  << std::endl;
	}
}

static void printV3DInfo()
{
	std::cout << "V3D Status Register Info:" << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "V3D revision:" << std::setw(VAL_LENGTH) << V3D::instance().getSystemInfo(SystemInfo::V3D_REVISION) << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "HDR support:" << std::setw(VAL_LENGTH) << (V3D::instance().getSystemInfo(SystemInfo::HDR_SUPPORT) ? "yes" : "no") << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "Semaphores:" << std::setw(VAL_LENGTH) << V3D::instance().getSystemInfo(SystemInfo::SEMAPHORES_COUNT) << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "QPUs:" << std::setw(VAL_LENGTH) << V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT) << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "Slices:" << std::setw(VAL_LENGTH) << V3D::instance().getSystemInfo(SystemInfo::SLICES_COUNT) << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "VPM Memory size:" << std::setw(VAL_LENGTH) << V3D::instance().getSystemInfo(SystemInfo::VPM_MEMORY_SIZE) / 1024 << " KB" << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "VPM User size:" << std::setw(VAL_LENGTH) << V3D::instance().getSystemInfo(SystemInfo::VPM_USER_MEMORY_SIZE) / 1024 << " KB" << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "Program queue:" << std::setw(VAL_LENGTH) << ((std::to_string(V3D::instance().getSystemInfo(SystemInfo::USER_REQUESTS_COUNT)) + "/") +
			(std::to_string(V3D::instance().getSystemInfo(SystemInfo::USER_PROGRAMS_COMPLETED_COUNT)) + "/") + std::to_string(V3D::instance().getSystemInfo(SystemInfo::PROGRAM_QUEUE_LENGTH)))
			<< " requests/completed/in queue" << std::endl;
	std::cout << std::setw(NAME_LENGTH) << "Errors:" << std::setw(VAL_LENGTH) << getErrors() << std::endl;

	//TODO performance counters ??
}

static void printMaximumAllocation()
{
	Mailbox& mb = mailbox();
	uint32_t maxSize = mb.getTotalGPUMemory();

	std::cout << "Testing maximum single allocation size:" << std::endl;
	for(uint32_t trySize = maxSize; trySize > 1; trySize >>= 1)
	{
		std::unique_ptr<DeviceBuffer> buffer(mb.allocateBuffer(trySize));
		if(buffer != nullptr)
		{
			std::cout << "Maximum single allocation: " << buffer->size << " bytes (" << (buffer->size/1024)/1024 << " MB)" << std::endl;
			break;
		}
	}
}

int main(int argc, char** argv)
{
	std::cout << "V3D Info:" << std::endl;
	std::cout << std::endl;


	printMailboxInfo();
	printV3DInfo();
	printMaximumAllocation();
}
