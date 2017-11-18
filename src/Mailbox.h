/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code. See the statement below for the copyright of the original code:
 */

/*
 Copyright (c) 2012, Broadcom Europe Ltd.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of the copyright holder nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef VC4CL_MAILBOX
#define VC4CL_MAILBOX

#include "common.h"

#include <chrono>
#include <utility>
#include <vector>

#define PAGE_ALIGNMENT 4096

namespace vc4cl
{
	class Mailbox;

	/*
	 * Container for the various pointers required for a GPU buffer object
	 *
	 * This is a RAII wrapper around a GPU memory buffer
	 */
	struct DeviceBuffer
	{
	public:
		//Identifier of the buffer allocated, think of it as a file-handle
		const uint32_t memHandle;
		//Buffer address from VideoCore QPU (GPU) view (the pointer which is passed to the kernel)
		const uint32_t qpuPointer;
		//Buffer address for ARM (host) view (the pointer to use on the host-side to fill/read the buffer)
		void* const hostPointer;
		//size of the buffer, in bytes
		const uint32_t size;

		DeviceBuffer(const DeviceBuffer&) = delete;
		DeviceBuffer(DeviceBuffer&&) = delete;
		~DeviceBuffer();

		DeviceBuffer& operator=(const DeviceBuffer&) = delete;
		DeviceBuffer& operator=(DeviceBuffer&&) = delete;
	private:
		DeviceBuffer(uint32_t handle, uint32_t devPtr, void* hostPtr, uint32_t size);

		friend class Mailbox;
	};

	enum MemoryFlag
	{
		//taken from https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
		DISCARDABLE = 1 << 0, /* can be resized to 0 at any time. Use for cached data */
		NORMAL = 0 << 2, /* normal allocating alias. Don't use from ARM */
		DIRECT = 1 << 2, /* 0x4 alias uncached */
		COHERENT = 2 << 2, /* 0x8 alias. Non-allocating in L2 but coherent */
		L1_NONALLOCATING = (DIRECT | COHERENT), /* Allocating in L2 */
		ZERO = 1 << 4,  /* initialise buffer to all zeros */
		NO_INIT = 1 << 5, /* don't initialise (default is initialise to all ones */
		HINT_PERMALOCK = 1 << 6 /* Likely to be locked for long periods of time. */
	};

	/*
	 * For all tags and their meaning / parameters, see:
	 * https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
	 *
	 */
	enum MailboxTag : unsigned
	{
		 FIRMWARE_REVISION = 0x00000001,
		 BOARD_MODEL = 0x00010001,
		 BOARD_REVISION = 0x00010002,
		 MAC_ADDRESS = 0x00010003,
		 BOARD_SERIAL = 0x00010004,
		 ARM_MEMORY = 0x00010005,
		 VC_MEMORY = 0x00010006,
		 CLOCKS = 0x00010007,
		 COMMAND_LINE = 0x00050001,
		 DMA_CHANNELS = 0x00060001,
		 GET_POWER_STATE = 0x00020001,
		 TIMING = 0x00020002,
		 SET_POWER_STATE = 0x0002801,
		 GET_CLOCK_STATE = 0x00030001,
		 SET_CLOCK_STATE = 0x00038001,
		 GET_CLOCK_RATE = 0x00030002,
		 SET_CLOCK_RATE = 0x00038002,
		 GET_MAX_CLOCK_RATE = 0x00030004,
		 GET_MIN_CLOCK_RATE = 0x00030007,
		 GET_TURBO_STATE = 0x00030009,
		 SET_TURBO_STATE = 0x00038009,
		 GET_VOLTAGE = 0x00030003,
		 SET_VOLTAGE = 0x00038003,
		 GET_MAX_VOLTAGE = 0x00030005,
		 GET_MIN_VOLTAGE = 0x00030008,
		 GET_TEMPERATURE = 0x00030006,
		 GET_MAX_TEMPERATURE = 0x0003000A,
		 ALLOCATE_MEMORY = 0x0003000C,
		 LOCK_MEMORY = 0x0003000D,
		 UNLOCK_MEMORY = 0x0003000E,
		 RELEASE_MEMORY = 0x0003000F,
		 EXECUTE_CODE = 0x00030010
	};

	class Mailbox
	{
	public:

		Mailbox();
		//disallow copy, since one may close the other's file descriptor
		Mailbox(const Mailbox&) = delete;
		//disallow move, since we use a singleton
		Mailbox(Mailbox&&) = delete;
		~Mailbox();

		Mailbox& operator=(const Mailbox&) = delete;
		Mailbox& operator=(Mailbox&&) = delete;

		DeviceBuffer* allocateBuffer(unsigned sizeInBytes, unsigned alignmentInBytes = PAGE_ALIGNMENT, MemoryFlag flags = MemoryFlag::L1_NONALLOCATING) const;
		bool deallocateBuffer(const DeviceBuffer* buffer) const;

		CHECK_RETURN bool executeCode(uint32_t codeAddress, unsigned valueR0, unsigned valueR1, unsigned valueR2, unsigned valueR3, unsigned valueR4, unsigned valueR5) const;
		CHECK_RETURN bool executeQPU(unsigned numQPUs, std::pair<uint32_t*, uint32_t> controlAddress, bool flushBuffer, std::chrono::milliseconds timeout) const;
		uint32_t getTotalGPUMemory() const;

		CHECK_RETURN bool readMailbox(MailboxTag tag, unsigned bufferLength, const std::vector<unsigned>& requestData, std::vector<unsigned>& resultData) const;

	private:
		int fd;

		CHECK_RETURN int mailboxCall(void* buffer) const;

		CHECK_RETURN bool enableQPU(bool enable) const;

		unsigned memAlloc(unsigned sizeInBytes, unsigned alignmentInBytes, MemoryFlag flags) const;
		unsigned memLock(unsigned handle) const;

		CHECK_RETURN bool memUnlock(unsigned handle) const;
		CHECK_RETURN bool memFree(unsigned handle) const;
	};

	Mailbox& mailbox();

	enum class VC4Clock
	{
		RESERVED = 0,
		EMMC  = 1,
		UART = 2,
		ARM = 3,
		CORE = 4,
		V3D = 5,
		H264 = 6,
		ISP = 7,
		SDRAM = 8,
		PIXEL = 9,
		PWM = 10
	};

} /* namespace vc4cl */

#endif /* VC4CL_MAILBOX */
