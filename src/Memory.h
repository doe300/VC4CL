/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code. See the statement below for the copyright of the
 * original code:
 */

#ifndef VC4CL_DEVICE_BUFFER
#define VC4CL_DEVICE_BUFFER

#include <cstdint>
#include <memory>
#include <ostream>

namespace vc4cl
{
    class SystemAccess;

    struct DevicePointer
    {
    public:
        constexpr explicit DevicePointer(uint32_t ptr) : pointer(ptr) {}

        constexpr explicit operator uint32_t() const
        {
            return pointer;
        }

    private:
        uint32_t pointer;

        friend std::ostream& operator<<(std::ostream& s, const DevicePointer& ptr);
    };

    std::ostream& operator<<(std::ostream& s, const DevicePointer& ptr);

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

        DeviceBuffer(const std::shared_ptr<SystemAccess>& sys, uint32_t handle, DevicePointer devPtr, void* hostPtr,
            uint32_t size);
        DeviceBuffer(const DeviceBuffer&) = delete;
        DeviceBuffer(DeviceBuffer&&) = delete;
        ~DeviceBuffer();

        DeviceBuffer& operator=(const DeviceBuffer&) = delete;
        DeviceBuffer& operator=(DeviceBuffer&&) = delete;

        void dumpContent() const;

    private:
        std::shared_ptr<SystemAccess> system;
    };
} // namespace vc4cl

#endif /* VC4CL_DEVICE_BUFFER */
