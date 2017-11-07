/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code. See the copyright statement below for the original code:
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

#include "Mailbox.h"
#include "V3D.h"

#include <sys/ioctl.h>
#include <system_error>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdio.h>
#include <memory>
#include <iostream>

using namespace vc4cl;

#define MAJOR_NUM 100
#define IOCTL_MBOX_PROPERTY _IOWR(MAJOR_NUM, 0, char *)
#define DEVICE_FILE_NAME "/dev/vcio"
#define GPU_MEM_MAP 0x0 // cached=0x0; direct=0x20000000

DeviceBuffer::DeviceBuffer(uint32_t handle, uint32_t devPtr, void* hostPtr, uint32_t size) : memHandle(handle), qpuPointer(devPtr), hostPointer(hostPtr), size(size)
{
}

DeviceBuffer::~DeviceBuffer()
{
	if(memHandle != 0)
		mailbox().deallocateBuffer(this);
}

static int mbox_open()
{
	int file_desc;

	// open a char device file used for communicating with kernel mbox driver
	file_desc = open(DEVICE_FILE_NAME, 0);
	if (file_desc < 0)
	{
		std::cout << "[VC4CL] Can't open device file: " <<  DEVICE_FILE_NAME << std::endl;
		std::cout << "[VC4CL] Try creating a device file with: sudo mknod " << DEVICE_FILE_NAME << " c " << MAJOR_NUM << " 0" << std::endl;
		throw std::system_error(errno, std::system_category(), "Failed to open mailbox");
	}
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Mailbox file descriptor opened: " << file_desc << std::endl;
#endif
	return file_desc;
}

Mailbox::Mailbox() : fd(mbox_open())
{
	if(!enableQPU(true))
		throw std::runtime_error("Failed to enable QPUs!");
}

Mailbox::~Mailbox()
{
	ignoreReturnValue(enableQPU(false), __FILE__, __LINE__, "There is no way of handling an error here");
	close(fd);
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Mailbox file descriptor closed: " << fd << std::endl;
#endif
}

DeviceBuffer* Mailbox::allocateBuffer(unsigned sizeInBytes, unsigned alignmentInBytes, MemoryFlag flags) const
{
	//munmap requires an alignment of the system page size (4096), so we need to enforce it here
	unsigned handle = memAlloc(sizeInBytes, std::max(static_cast<unsigned>(PAGE_ALIGNMENT), alignmentInBytes), flags);
	if(handle != 0)
	{
		unsigned qpuPointer = memLock(handle);
		void* hostPointer = mapmem(V3D::busAddressToPhysicalAddress(qpuPointer + GPU_MEM_MAP), sizeInBytes);
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Allocated " << sizeInBytes << " bytes of buffer: handle " << handle << ", device address " << qpuPointer << ", host address " << hostPointer << std::endl;
#endif
		return new DeviceBuffer(handle, qpuPointer, hostPointer, sizeInBytes);
	}
	return nullptr;
}

bool Mailbox::deallocateBuffer(const DeviceBuffer* buffer) const
{
	if(buffer->hostPointer != NULL)
		unmapmem(buffer->hostPointer, buffer->size);
	if(buffer->memHandle != 0)
	{
		if(!memUnlock(buffer->memHandle))
			return false;
		if(!memFree(buffer->memHandle))
			return false;
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Deallocated " << buffer->size << " bytes of buffer: handle " << buffer->memHandle << ", device address " << buffer->qpuPointer << ", host address " << buffer->hostPointer << std::endl;
#endif
	}
	return true;
}

bool Mailbox::executeCode(void* codeAddress, unsigned valueR0, unsigned valueR1, unsigned valueR2, unsigned valueR3, unsigned valueR4, unsigned valueR5) const
{
	int i=0;
	unsigned p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x30010; // (the tag id)
	p[i++] = 28; // (size of the buffer)
	p[i++] = 28; // (size of the data)
	p[i++] = reinterpret_cast<uintptr_t>(codeAddress);
	p[i++] = valueR0;
	p[i++] = valueR1;
	p[i++] = valueR2;
	p[i++] = valueR3;
	p[i++] = valueR4;
	p[i++] = valueR5;

	p[i++] = 0x00000000; // end tag
	p[0] = i*sizeof *p; // actual size

	if(mailboxCall(p) < 0)
		return false;
	return p[5] == 0;
}

bool Mailbox::executeQPU(unsigned numQPUs, std::pair<uint32_t*, uintptr_t> controlAddress, bool flushBuffer, std::chrono::milliseconds timeout) const
{
	if(timeout.count() > 0xFFFFFFFF)
	{
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Timeout is too big, needs fit into a 32-bit integer: " << timeout.count() << std::endl;
		return false;
#endif
	}
	int i=0;
	unsigned p[32];

	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request
	p[i++] = 0x30011; // (the tag id)
	p[i++] = 16; // (size of the buffer)
	p[i++] = 16; // (size of the data)
	p[i++] = numQPUs;
	p[i++] = controlAddress.second;
	p[i++] = !flushBuffer;
	p[i++] = timeout.count(); // ms

	p[i++] = 0x00000000; // end tag
	p[0] = i*sizeof *p; // actual size

	if(mailboxCall(p) < 0)
		return false;
	return p[5] == 0;
}

uint32_t Mailbox::getTotalGPUMemory() const
{
	//we set it to half of the available graphics memory, to reserve some space for kernels/video calculations
	std::vector<uint32_t> result;
	if(!readMailbox(MailboxTag::VC_MEMORY, 2, {}, result))
		return 0;
	return result.at(1) / 2;
}

bool Mailbox::readMailbox(const MailboxTag tag, const unsigned bufferLength, const std::vector<unsigned>& requestData, std::vector<unsigned>& resultData) const
{
	int i=0;
	unsigned p[32];

	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = tag; // (the tag id)
	p[i++] = bufferLength * sizeof(unsigned); // (size of the buffer in bytes)
	p[i++] = requestData.size() * sizeof(unsigned); // (size of the data in bytes)
	for(unsigned u : requestData)
		p[i++] = u;
	//fill with empty space, if buffer-length > request_length
	for(unsigned j = requestData.size(); j < bufferLength; ++j)
		p[i++] = 0x00000000;	//empty space for return values

	p[i++] = 0x00000000; // end tag
	p[0] = i * sizeof(unsigned); // actual size

	if(mailboxCall(p) < 0)
		return false;
	resultData.clear();
	resultData.reserve(bufferLength);
	for(size_t i = 0; i < bufferLength; ++i)
	{
		//p[5] is the first content field
		resultData.push_back(p[5 + i]);
	}

	if(p[1] >> 31)	//0x8000000x
	{
		//0x80000000 on success
		//0x80000001 on failure
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Mailbox request: " << ((p[1] & 0x1) ? "failed" : "succeeded") << std::endl;
#endif
		return p[1]  == 0x80000000;
	}
	else
	{
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Unknown return code: " << p[1] << std::endl;
#endif
		return false;
	}
}

/*
 * use ioctl to send mbox property message
 */
int Mailbox::mailboxCall(void *buffer) const
{
#ifdef DEBUG_MODE
	unsigned* p = (unsigned*)buffer;
	unsigned size = *((unsigned *)buffer);
	std::cout << "[VC4CL] Mailbox buffer before:" << std::endl;
	for (unsigned i = 0; i < size / 4; ++i)
		printf("[VC4CL] %04zx: 0x%08x\n", i*sizeof *p, p[i]);
#endif

	int ret_val = ioctl(fd, IOCTL_MBOX_PROPERTY, buffer);
	if (ret_val < 0)
	{
		std::cout << "[VC4CL] ioctl_set_msg failed: " << ret_val << std::endl;
		perror("[VC4CL] Error in mbox_property");
		throw std::system_error(errno, std::system_category(), "Failed to set mailbox property");
	}

#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Mailbox buffer after:" << std::endl;
	for (unsigned i = 0; i < size / 4; ++i)
		printf("[VC4CL] %04zx: 0x%08x\n", i*sizeof *p, p[i]);
	std::cout << std::endl;
#endif
	return ret_val;
}

bool Mailbox::enableQPU(bool enable) const
{
	int i=0;
	unsigned p[32];

	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x30012; // (the tag id)
	p[i++] = 4; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = enable;

	p[i++] = 0x00000000; // end tag
	p[0] = i*sizeof *p; // actual size

	if(mailboxCall(p) < 0)
		return false;
	return p[5] == 0;
}

unsigned Mailbox::memAlloc(unsigned sizeInBytes, unsigned alignmentInBytes, MemoryFlag flags) const
{
	int i=0;
	unsigned p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x3000c; // (the tag id)
	p[i++] = 12; // (size of the buffer)
	p[i++] = 12; // (size of the data)
	p[i++] = sizeInBytes; // (num bytes? or pages?)
	p[i++] = alignmentInBytes; // (alignment)
	p[i++] = flags; // (MEM_FLAG_L1_NONALLOCATING)

	p[i++] = 0x00000000; // end tag
	p[0] = i*sizeof *p; // actual size

	if(mailboxCall(p) < 0)
		return 0;
	return p[5];
}

unsigned Mailbox::memLock(unsigned handle) const
{
	int i=0;
   unsigned p[32];
   p[i++] = 0; // size
   p[i++] = 0x00000000; // process request

   p[i++] = 0x3000d; // (the tag id)
   p[i++] = 4; // (size of the buffer)
   p[i++] = 4; // (size of the data)
   p[i++] = handle;

   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof *p; // actual size

   if(mailboxCall(p) < 0)
	   return reinterpret_cast<uintptr_t>(nullptr);
   return p[5];
}

bool Mailbox::memUnlock(unsigned handle) const
{
	int i=0;
	unsigned p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x3000e; // (the tag id)
	p[i++] = 4; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = handle;

	p[i++] = 0x00000000; // end tag
	p[0] = i*sizeof *p; // actual size

	if(mailboxCall(p) < 0)
		return false;
	return p[5] == 0;
}

bool Mailbox::memFree(unsigned handle) const
{
	int i=0;
	unsigned p[32];
	p[i++] = 0; // size
	p[i++] = 0x00000000; // process request

	p[i++] = 0x3000f; // (the tag id)
	p[i++] = 4; // (size of the buffer)
	p[i++] = 4; // (size of the data)
	p[i++] = handle;

	p[i++] = 0x00000000; // end tag
	p[0] = i*sizeof *p; // actual size

	if(mailboxCall(p) < 0)
		return false;
	return p[5] == 0;
}

static std::unique_ptr<Mailbox> mb;

Mailbox& vc4cl::mailbox()
{
	if(mb.get() == nullptr)
	{
		mb.reset(new Mailbox());
	}
	return *mb.get();
}
