/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code. See the statement below for the copyright of the
 * original code:
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
#include "executor.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#define PAGE_ALIGNMENT 4096

namespace vc4cl
{
    class Mailbox;

    struct DevicePointer
    {
    public:
        constexpr explicit DevicePointer(uint32_t ptr) : pointer(ptr) {}

        constexpr explicit operator uint32_t() const
        {
            return pointer;
        }

        friend std::ostream& operator<<(std::ostream& s, const DevicePointer& ptr)
        {
            return s << ptr.pointer;
        }

    private:
        uint32_t pointer;
    };

    /*
     * Container for the various pointers required for a GPU buffer object
     *
     * This is a RAII wrapper around a GPU memory buffer
     */
    struct DeviceBuffer
    {
    public:
        // Identifier of the buffer allocated, think of it as a file-handle
        const uint32_t memHandle;
        // Buffer address from VideoCore QPU (GPU) view (the pointer which is passed to the kernel)
        const DevicePointer qpuPointer;
        // Buffer address for ARM (host) view (the pointer to use on the host-side to fill/read the buffer)
        void* const hostPointer;
        // size of the buffer, in bytes
        const uint32_t size;

        DeviceBuffer(const DeviceBuffer&) = delete;
        DeviceBuffer(DeviceBuffer&&) = delete;
        ~DeviceBuffer();

        DeviceBuffer& operator=(const DeviceBuffer&) = delete;
        DeviceBuffer& operator=(DeviceBuffer&&) = delete;

        void dumpContent() const;

    private:
        DeviceBuffer(
            const std::shared_ptr<Mailbox>& mb, uint32_t handle, DevicePointer devPtr, void* hostPtr, uint32_t size);

        std::shared_ptr<Mailbox> mailbox;

        friend class Mailbox;
    };

    // taken from https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
    // additional documentation from:
    // https://github.com/raspberrypi/userland/blob/master/vcfw/rtos/common/rtos_common_mem.h
    enum MemoryFlag
    {
        /*
         * If a handle is discardable, the memory manager may resize it to size 0 at any time when it is not locked or
         * retained.
         */
        DISCARDABLE = 1 << 0,
        /*
         * Block must be kept within bottom 256M region of the relocatable heap.
         * Specifying this flag means that an allocation will fail if the block cannot be allocated within that region,
         * and the block will not be moved out of that range.
         *
         * (This is to support memory blocks used by the codec cache, which must have same top 4 bits; see HW-3058)
         */
        MEM_FLAG_LOW_256M = 1 << 1,
        /*
         * If a handle is allocating (or normal), its block of memory will be accessed in an allocating fashion through
         * the cache.
         *
         * normal allocating alias. Don't use from ARM
         */
        NORMAL = 0 << 2,
        /*
         * If a handle is direct, its block of memory will be accessed directly, bypassing the cache.
         */
        DIRECT = 1 << 2,
        /*
         * If a handle is coherent, its block of memory will be accessed in a non-allocating fashion through the cache.
         */
        COHERENT = 2 << 2,
        /*
         * If a handle is L1-nonallocating, its block of memory will be accessed by the VPU in a fashion which is
         * allocating in L2, but only coherent in L1.
         */
        L1_NONALLOCATING = (DIRECT | COHERENT),
        /*
         * If a handle is zero'd, its contents are initialized to 0
         */
        ZERO = 1 << 4,
        /*
         * If a handle is uninitialized, it will not be reset to a defined value (either zero, or all 1's) on
         * allocation.
         *
         * don't initialize (default is initialize to all ones)
         */
        NO_INIT = 1 << 5,
        /*
         * Likely to be locked for long periods of time.
         */
        HINT_PERMALOCK = 1 << 6
    };

    /*
     * For all tags and their meaning / parameters, see:
     * https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface (incomplete list)
     * https://github.com/raspberrypi/linux/blob/rpi-4.9.y/include/soc/bcm2835/raspberrypi-firmware.h
     *
     * The user-space access via ioctl goes through:
     * https://github.com/raspberrypi/linux/blob/rpi-4.9.y/drivers/char/broadcom/vcio.c
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
        SET_POWER_STATE = 0x00028001,
        GET_CLOCK_STATE = 0x00030001,
        SET_CLOCK_STATE = 0x00038001,
        GET_CLOCK_RATE = 0x00030002,
        // clock rate actually measured from the hardware
        GET_CLOCK_RATE_MEASURED = 0x00030047,
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
        // 1. word time in microseconds since VideoCore boot, 2. word unused
        GET_STC = 0x0003000B,
        ALLOCATE_MEMORY = 0x0003000C,
        LOCK_MEMORY = 0x0003000D,
        UNLOCK_MEMORY = 0x0003000E,
        RELEASE_MEMORY = 0x0003000F,
        EXECUTE_CODE = 0x00030010,
        EXECUTE_QPU = 0x00030011,
        ENABLE_QPU = 0x00030012,
        GET_THROTTLED = 0x00030046
    };

    /*
     * 0: Buffer size
     * 1: request/response code
     * 2: request tag
     * 3: content size
     * 4: request/response data size
     * ...: request/response data
     * x: end tag (0)
     *
     */
    template <MailboxTag Tag, unsigned RequestSize, unsigned MaxResponseSize>
    struct MailboxMessage
    {
        static constexpr unsigned requestSize = RequestSize;
        static constexpr unsigned maximumResponseSize = MaxResponseSize;
        static constexpr unsigned contentSize = requestSize > maximumResponseSize ? requestSize : maximumResponseSize;
        static constexpr unsigned messageSize = contentSize + 6;
        static constexpr MailboxTag tag = Tag;

        std::array<unsigned, messageSize> buffer = {0};

        explicit MailboxMessage(std::array<unsigned, requestSize> request)
        {
            static_assert(RequestSize > 0, "For empty requests, use the default constructor!");
            buffer[0] = static_cast<unsigned>(buffer.size() * sizeof(unsigned));
            buffer[1] = 0; // this is a request
            buffer[2] = static_cast<unsigned>(tag);
            buffer[3] = static_cast<unsigned>(contentSize * sizeof(unsigned));
            buffer[4] = static_cast<unsigned>(requestSize * sizeof(unsigned));
            memcpy(&buffer[5], request.data(), request.size() * sizeof(unsigned));
        }

        explicit MailboxMessage()
        {
            static_assert(RequestSize == 0, "For non-empty requests, use the parameterized constructor!");
            buffer[0] = static_cast<unsigned>(buffer.size() * sizeof(unsigned));
            buffer[1] = 0; // this is a request
            buffer[2] = static_cast<unsigned>(tag);
            buffer[3] = static_cast<unsigned>(contentSize * sizeof(unsigned));
            buffer[4] = static_cast<unsigned>(requestSize * sizeof(unsigned));
        }

        inline unsigned getResponseValue() const
        {
            return buffer[1];
        }

        inline bool isResponse() const
        {
            return buffer[1] == 0x80000000 || buffer[1] == 0x80000001;
        }

        inline bool isSuccessful() const
        {
            return buffer[1] == 0x80000000;
        }

        inline unsigned getResponseSize() const
        {
            // first bit set indicates response
            return buffer[4] & 0x7FFFFFFF;
        }

        inline unsigned* getContent()
        {
            return &buffer[5];
        }

        inline unsigned getContent(unsigned index) const
        {
            return buffer.at(5 + index);
        }
    };

    template <MailboxTag Tag>
    using SimpleQueryMessage =
        MailboxMessage<Tag, 0 /* no additional request data */, 2 /* one or two response values */>;
    template <MailboxTag Tag>
    using QueryMessage = MailboxMessage<Tag, 1 /* single request value */, 2 /* one or two response values */>;

    class Mailbox : public std::enable_shared_from_this<Mailbox>
    {
    public:
        Mailbox();
        // disallow copy, since one may close the other's file descriptor
        Mailbox(const Mailbox&) = delete;
        // disallow move, since we use a singleton
        Mailbox(Mailbox&&) = delete;
        ~Mailbox();

        Mailbox& operator=(const Mailbox&) = delete;
        Mailbox& operator=(Mailbox&&) = delete;

        DeviceBuffer* allocateBuffer(unsigned sizeInBytes, unsigned alignmentInBytes = PAGE_ALIGNMENT,
            MemoryFlag flags = MemoryFlag::L1_NONALLOCATING);
        bool deallocateBuffer(const DeviceBuffer* buffer) const;

        CHECK_RETURN ExecutionHandle executeCode(uint32_t codeAddress, unsigned valueR0, unsigned valueR1,
            unsigned valueR2, unsigned valueR3, unsigned valueR4, unsigned valueR5) const;
        CHECK_RETURN ExecutionHandle executeQPU(unsigned numQPUs, std::pair<uint32_t*, uint32_t> controlAddress,
            bool flushBuffer, std::chrono::milliseconds timeout) const;
        uint32_t getTotalGPUMemory() const;

        template <MailboxTag Tag, unsigned RequestSize, unsigned MaxResponseSize>
        bool readMailboxMessage(MailboxMessage<Tag, RequestSize, MaxResponseSize>& message) const
        {
            if(mailboxCall(message.buffer.data()) < 0)
                return false;
            return checkReturnValue(message.getResponseValue());
        }

    private:
        int fd;

        CHECK_RETURN int mailboxCall(void* buffer) const;

        CHECK_RETURN bool enableQPU(bool enable) const;

        unsigned memAlloc(unsigned sizeInBytes, unsigned alignmentInBytes, MemoryFlag flags) const;
        DevicePointer memLock(unsigned handle) const;

        CHECK_RETURN bool memUnlock(unsigned handle) const;
        CHECK_RETURN bool memFree(unsigned handle) const;

        CHECK_RETURN bool readMailboxMessage(unsigned* buffer, unsigned bufferSize);

        CHECK_RETURN bool checkReturnValue(unsigned value) const __attribute__((const));
    };

    std::shared_ptr<Mailbox>& mailbox();

    enum class VC4Clock
    {
        RESERVED = 0,
        EMMC = 1,
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
