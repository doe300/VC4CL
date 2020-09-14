/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef VC4CL_BUFFER
#define VC4CL_BUFFER

#include "CommandQueue.h"
#include "Event.h"
#include "Mailbox.h"
#include "Object.h"

#include <list>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace vc4cl
{
    using BufferDestructionCallback = void(CL_CALLBACK*)(cl_mem event, void* user_data);

    class Image;
    struct BufferMapping;

    /**
     * We need to keep track of some more information for our mapped pointers for several reasons:
     *
     * - need to handle multiple calls to clEnqueueMapBuffer() mapped to the same host pointer, i.e. as guaranteed
     * by CL_MEM_USE_HOST_PTR, moreover need to make sure we only unmap a single one of those mappings and also just
     * once!
     * - if the user does not care about the previous contents of the buffer, we can skip copying the data from the
     * device to the host buffer.
     * - if the mapping was read-only, we can skip copying the contents back from the host to the device buffer on
     * unmapping.
     */
    struct MappingInfo
    {
        // The pointer to which this mapping is mapped to
        void* hostPointer;
        // Whether this mapping has already been requested to be unmapped
        bool unmapScheduled;
        // Whether the contents of the mapped host buffer do not need to be copied from the device buffer at mapping
        // time
        bool skipPopulatingBuffer;
        // Whether there is no need to write any data back to the device buffer on unmapping, e.g. the mapping is
        // read-only
        bool skipWritingBack;
    };

    class Buffer : public Object<_cl_mem, CL_INVALID_MEM_OBJECT>, public HasContext
    {
    public:
        Buffer(Context* context, cl_mem_flags flags);
        Buffer(Buffer* parent, cl_mem_flags flags);
        ~Buffer() override;

        Buffer* createSubBuffer(cl_mem_flags flags, cl_buffer_create_type buffer_create_type,
            const void* buffer_create_info, cl_int* errcode_ret);
        CHECK_RETURN cl_int enqueueRead(CommandQueue* commandQueue, bool blockingRead, size_t offset, size_t size,
            void* ptr, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueWrite(CommandQueue* commandQueue, bool blockingWrite, size_t offset, size_t size,
            const void* ptr, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueReadRect(CommandQueue* commandQueue, bool blockingRead, const size_t* bufferOrigin,
            const size_t* hostOrigin, const size_t* region, size_t bufferRowPitch, size_t bufferSlicePitch,
            size_t hostRowPitch, size_t hostSlicePitch, void* ptr, cl_uint numEventsInWaitList,
            const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueWriteRect(CommandQueue* commandQueue, bool blockingWrite, const size_t* bufferOrigin,
            const size_t* hostOrigin, const size_t* region, size_t bufferRowPitch, size_t bufferSlicePitch,
            size_t hostRowPitch, size_t hostSlicePitch, const void* ptr, cl_uint numEventsInWaitList,
            const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueCopyInto(CommandQueue* commandQueue, Buffer* destination, size_t srcOffset,
            size_t dstOffset, size_t size, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueCopyIntoRect(CommandQueue* commandQueue, Buffer* destination,
            const size_t* srcOrigin, const size_t* dstOrigin, const size_t* region, size_t srcRowPitch,
            size_t srcSlicePitch, size_t dstRowPitch, size_t dstSlicePitch, cl_uint numEventsInWaitList,
            const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueFill(CommandQueue* commandQueue, const void* pattern, size_t patternSize,
            size_t offset, size_t size, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
        CHECK_RETURN void* enqueueMap(CommandQueue* commandQueue, bool blockingMap, cl_map_flags mapFlags,
            size_t offset, size_t size, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event,
            cl_int* errcode_ret);
        CHECK_RETURN cl_int setDestructorCallback(BufferDestructionCallback callback, void* userData);
        CHECK_RETURN cl_int enqueueUnmap(CommandQueue* commandQueue, void* mappedPtr, cl_uint numEventsInWaitList,
            const cl_event* waitList, cl_event* event);
        CHECK_RETURN virtual cl_int getInfo(
            cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

        void setUseHostPointer(void* hostPtr, size_t hostSize);
        void setAllocateHostPointer(size_t hostSize);
        void setCopyHostPointer(void* hostPtr, size_t hostSize);
        cl_mem_flags getMemFlags() const __attribute__((pure));

        /*
         * For use with CL_MEM_USE_HOST_PTR, this synchronizes the data in the device-buffer with the host-buffer
         */
        CHECK_RETURN cl_int copyIntoHostBuffer(size_t offset, size_t size);
        CHECK_RETURN cl_int copyFromHostBuffer(size_t offset, size_t size);

        bool readable;
        bool writeable;
        bool hostReadable;
        bool hostWriteable;

        std::shared_ptr<DeviceBuffer> deviceBuffer;

        void setHostSize();

        DevicePointer getDevicePointerWithOffset();
        void* getDeviceHostPointerWithOffset();

    protected:
        bool useHostPtr;
        bool allocHostPtr;
        bool copyHostPtr;

        // the pointer to the beginning of the host-side cached data, for sub-buffers, this is the start of the
        // sub-buffer
        void* hostPtr = nullptr;
        // the actual size of the buffer, can be less than the device-buffer size (e.g. for sub-buffers)
        size_t hostSize = 0;

        mutable std::mutex mappingsLock;
        std::list<MappingInfo> mappings;

        std::vector<std::pair<BufferDestructionCallback, void*>> callbacks;

        object_wrapper<Buffer> parent;
        size_t subBufferOffset = 0;

        CHECK_RETURN Event* createBufferActionEvent(CommandQueue* queue, CommandType command_type,
            cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_int* errcode_ret) const;

        friend class Image;
        friend struct BufferMapping;
    };

    struct BufferMapping : public EventAction
    {
        object_wrapper<Buffer> buffer;
        std::list<MappingInfo>::const_iterator mappingInfo;
        bool unmap;

        BufferMapping(Buffer* buffer, std::list<MappingInfo>::const_iterator mappingInfo, bool unmap);
        ~BufferMapping() override;

        cl_int operator()() override final;
    };

    struct BufferAccess : public EventAction
    {
        object_wrapper<Buffer> buffer;
        std::size_t bufferOffset;
        void* hostPtr;
        std::size_t hostOffset;
        std::size_t numBytes;
        bool writeToBuffer;

        BufferAccess(Buffer* buffer, void* hostPtr, std::size_t numBytes, bool writeBuffer);
        ~BufferAccess() override;

        cl_int operator()() override;
    };

    struct BufferRectAccess final : public BufferAccess
    {
        std::array<std::size_t, 3> region;
        std::array<std::size_t, 3> bufferOrigin;
        std::size_t bufferRowPitch;
        std::size_t bufferSlicePitch;
        std::array<std::size_t, 3> hostOrigin;
        std::size_t hostRowPitch;
        std::size_t hostSlicePitch;

        BufferRectAccess(Buffer* buf, void* hostPointer, const std::size_t region[3], bool writeBuffer);
        ~BufferRectAccess() override;

        cl_int operator()() override final;
    };

    struct BufferFill final : public EventAction
    {
        object_wrapper<Buffer> buffer;
        std::size_t bufferOffset;
        std::vector<char> pattern;
        std::size_t numBytes;

        BufferFill(Buffer* buffer, const void* pattern, std::size_t patternSize, std::size_t numBytes);
        ~BufferFill() override;

        cl_int operator()() override final;
    };

    struct BufferCopy final : public EventAction
    {
        object_wrapper<Buffer> sourceBuffer;
        std::size_t sourceOffset;
        object_wrapper<Buffer> destBuffer;
        std::size_t destOffset;
        std::size_t numBytes;

        BufferCopy(Buffer* src, Buffer* dest, std::size_t numBytes);
        ~BufferCopy() override;

        cl_int operator()() override final;
    };

    struct BufferRectCopy final : public EventAction
    {
        object_wrapper<Buffer> sourceBuffer;
        object_wrapper<Buffer> destBuffer;
        std::array<std::size_t, 3> region;
        std::array<std::size_t, 3> sourceOrigin;
        std::size_t sourceRowPitch;
        std::size_t sourceSlicePitch;
        std::array<std::size_t, 3> destOrigin;
        std::size_t destRowPitch;
        std::size_t destSlicePitch;

        BufferRectCopy(Buffer* src, Buffer* dest, const std::size_t region[3]);
        ~BufferRectCopy() override;

        cl_int operator()() override final;
    };

} /* namespace vc4cl */

#endif /* VC4CL_BUFFER */
