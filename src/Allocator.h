/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code. See the statement below for the copyright of the
 * original code:
 */
#ifndef VC4CL_ALLOCATOR_H
#define VC4CL_ALLOCATOR_H

#include "Mailbox.h"

#include <cstdint>
#include <memory>

namespace vc4cl
{
    class DeviceBlock : public std::enable_shared_from_this<DeviceBlock>
    {
    public:
        DeviceBlock(const std::shared_ptr<Mailbox>& mb, uint32_t handle, DevicePointer devPtr, void* hostPtr,
            uint32_t numBytes, MemoryFlag flags);
        DeviceBlock(const DeviceBlock&) = delete;
        DeviceBlock(DeviceBlock&&) noexcept = delete;

        ~DeviceBlock() noexcept;

        DeviceBlock& operator=(const DeviceBlock&) = delete;
        DeviceBlock& operator=(DeviceBlock&&) noexcept = delete;

        std::unique_ptr<DeviceBuffer> allocateBuffer(unsigned sizeInBytes, unsigned alignmentInBytes);

        void deallocateBuffer(void* ptr, uint32_t numBytes);

        const MemoryFlag flags;

    private:
        // Identifier of the buffer allocated, think of it as a file-handle
        const uint32_t memHandle;
        // Buffer address from VideoCore QPU (GPU) view (the pointer which is passed to the kernel)
        const DevicePointer qpuPointer;
        // Buffer address for ARM (host) view (the pointer to use on the host-side to fill/read the buffer)
        void* const hostPointer;
        // size of the buffer, in bytes
        const uint32_t size;
        std::shared_ptr<Mailbox> mailbox;
        // The host pointer to the next free address
        uint32_t freeOffset;
    };

} // namespace vc4cl

#endif /* VC4CL_ALLOCATOR_H */