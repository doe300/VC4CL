/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_KERNEL
#define VC4CL_KERNEL

#include "Bitfield.h"
#include "Event.h"
#include "Program.h"

#include <bitset>
#include <map>
#include <memory>
#include <vector>

namespace vc4cl
{
    struct DeviceBuffer;
    struct DevicePointer;
    struct KernelArgument;
    class Buffer;
    class Mailbox;
    class V3D;

    class Kernel final : public Object<_cl_kernel, CL_INVALID_KERNEL>
    {
    public:
        Kernel(Program* program, const KernelInfo& info);
        ~Kernel() noexcept override;

        CHECK_RETURN cl_int setArg(cl_uint arg_index, size_t arg_size, const void* arg_value);
        CHECK_RETURN cl_int getInfo(
            cl_kernel_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
        CHECK_RETURN cl_int getWorkGroupInfo(cl_kernel_work_group_info param_name, size_t param_value_size,
            void* param_value, size_t* param_value_size_ret);
        CHECK_RETURN cl_int getArgInfo(cl_uint arg_index, cl_kernel_arg_info param_name, size_t param_value_size,
            void* param_value, size_t* param_value_size_ret);
        CHECK_RETURN cl_int enqueueNDRange(CommandQueue* commandQueue, cl_uint work_dim,
            const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size,
            cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);

        object_wrapper<Program> program;
        const KernelInfo info;

        std::vector<std::unique_ptr<KernelArgument>> args;
        std::bitset<kernel_config::MAX_PARAMETER_COUNT> argsSetMask;

    private:
        CHECK_RETURN cl_int allocateAndTrackBufferArguments(
            std::map<unsigned, std::unique_ptr<DeviceBuffer>>& tmpBuffers,
            std::map<unsigned, std::pair<std::shared_ptr<DeviceBuffer>, DevicePointer>>& persistentBuffers) const;
    };

    struct KernelArgument
    {
        virtual ~KernelArgument() noexcept;

        virtual std::string to_string() const = 0;
        virtual std::unique_ptr<KernelArgument> clone() const = 0;
    };

    /**
     * Simple kernel argument storing vectors (1 to 16 elements) of scalar data
     */
    struct ScalarArgument final : public KernelArgument
    {
        ScalarArgument(uint32_t numEntries)
        {
            scalarValues.reserve(numEntries);
        }

        ~ScalarArgument() noexcept override;

        struct ScalarValue : private Bitfield<uint32_t>
        {
            BITFIELD_ENTRY(Float, float, 0, Int)
            BITFIELD_ENTRY(Unsigned, uint32_t, 0, Int)
            BITFIELD_ENTRY(Signed, int32_t, 0, Int)
        };
        std::vector<ScalarValue> scalarValues;

        void addScalar(float f);
        void addScalar(uint32_t u);
        void addScalar(int32_t s);
        void addScalar(uint64_t l);

        std::string to_string() const override;
        std::unique_ptr<KernelArgument> clone() const override;
    };

    /**
     * A kernel execution creates a temporary buffer (its lifetime is the duration of the kernel execution) for this
     * argument.
     *
     * Temporary buffers are required e.g. for local memory buffers as well as for direct input of complex data.
     */
    struct TemporaryBufferArgument final : public KernelArgument
    {
        TemporaryBufferArgument(unsigned bufferSize) : sizeToAllocate(bufferSize) {}
        TemporaryBufferArgument(unsigned bufferSize, const void* directData) :
            sizeToAllocate(bufferSize), data(bufferSize)
        {
            memcpy(data.data(), directData, bufferSize);
        }
        ~TemporaryBufferArgument() noexcept override;

        /*
         * This specifies the buffer-size to allocate, e.g. for __local pointers or direct struct parameters.
         *
         * NOTE: __local parameters are not passed a buffer, but the buffer-size to automatically allocate and
         * deallocate again after the kernel-execution.
         */
        const unsigned sizeToAllocate;

        /*
         * Passing non-trivial (e.g. struct) parameters directly to a kernel function generates pointers with
         * byval attribute set in LLVM. From the kernel side, they are treated as any other pointer parameter,
         * but on host side, they are set by directly passing the data, similar to direct vector parameters.
         *
         * We handle them by creating a buffer (similar to local memory), copying the data into this buffer and
         * passing the pointer to the kernel.
         */
        std::vector<uint8_t> data;

        std::string to_string() const override;
        std::unique_ptr<KernelArgument> clone() const override;
    };

    /**
     * The kernel argument refers to a preallocated buffer object
     */
    struct BufferArgument final : public KernelArgument
    {
        BufferArgument(Buffer* buffer) : buffer(buffer) {}
        ~BufferArgument() noexcept override;

        /*
         * NOTE: This is not a reference counting reference on purpose, since the OpenCL 1.2 standard forbids setting of
         * kernel arguments to increment the buffer's reference counter.
         *
         * Also this is a pointer on purpose, since the argument might be NULL
         *
         * See: https://www.khronos.org/registry/OpenCL/specs/2.2/html/OpenCL_API.html#_setting_kernel_arguments
         */
        Buffer* buffer;

        std::string to_string() const override;
        std::unique_ptr<KernelArgument> clone() const override;
    };

    struct KernelExecution final : public EventAction
    {
        object_wrapper<Kernel> kernel;
        // Keep a reference to the mailbox to guarantee it still exists when we actually do the execution. This is
        // mostly to gracefully handle application errors, e.g. when a kernel is executed and not waited for finished
        // and then the application shuts down.
        std::shared_ptr<Mailbox> mailbox;
        // Keep a reference to the V3D instance for the same reason as for the Mailbox above.
        std::shared_ptr<V3D> v3d;
        cl_uchar numDimensions;
        std::array<std::size_t, kernel_config::NUM_DIMENSIONS> globalOffsets;
        std::array<std::size_t, kernel_config::NUM_DIMENSIONS> globalSizes;
        std::array<std::size_t, kernel_config::NUM_DIMENSIONS> localSizes;

        /**
         * Tracks the state of the kernel arguments at the point this kernel execution event was created
         * (clEnqueueNDRangeKernel was called).
         *
         * We need to take a snapshot of the arguments and their values at the point of queuing the kernel execution,
         * since the arguments of the kernel object might be modified afterwards in preparation of executing the same
         * kernel object again.
         */
        std::vector<std::unique_ptr<KernelArgument>> executionArguments;

        /**
         * Tracks temporary and preexisting device buffers to guarantee they exist until the kernel finishes
         *
         * We start tracking them from the moment we create the KernelExecution event for following reasons:
         * - For temporary buffer, we can correctly return a failure to allocate enough resources
         * - For persistent buffers, we guarantee they are not freed until the execution actually starts.
         *   See also https://github.com/KhronosGroup/OpenCL-Docs/issues/45
         */
        std::map<unsigned, std::unique_ptr<DeviceBuffer>> tmpBuffers;
        // The value is the buffer + the actual address (buffer + offset, for sub-buffers)
        std::map<unsigned, std::pair<std::shared_ptr<DeviceBuffer>, DevicePointer>> persistentBuffers;

        explicit KernelExecution(Kernel* kernel);
        ~KernelExecution() override;

        cl_int operator()() override final;
    };

} /* namespace vc4cl */

#endif /* VC4CL_KERNEL */
