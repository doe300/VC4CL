/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef VC4CL_BUFFER
#define VC4CL_BUFFER

#include <utility>
#include <vector>
#include <memory>
#include <list>

#include "Object.h"
#include "CommandQueue.h"
#include "Event.h"
#include "Mailbox.h"

namespace vc4cl
{
	typedef void(CL_CALLBACK* BufferCallback)(cl_mem event, void* user_data);

	class Image;
	struct BufferMapping;

	class Buffer: public Object<_cl_mem, CL_INVALID_MEM_OBJECT>, public HasContext
	{
	public:
		Buffer(Context* context, cl_mem_flags flags);
		Buffer(Buffer* parent, cl_mem_flags flags);
		~Buffer();

		Buffer* createSubBuffer(cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const void* buffer_create_info, cl_int* errcode_ret);
		CHECK_RETURN cl_int enqueueRead(CommandQueue* commandQueue, bool blockingRead, size_t offset, size_t size, void* ptr, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN cl_int enqueueWrite(CommandQueue* commandQueue, bool blockingWrite, size_t offset, size_t size, const void* ptr, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN cl_int enqueueReadRect(CommandQueue* commandQueue, bool blockingRead, const size_t* bufferOrigin, const size_t* hostOrigin, const size_t* region, size_t bufferRowPitch, size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch, void* ptr, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN cl_int enqueueWriteRect(CommandQueue* commandQueue,bool blockingWrite, const size_t* bufferOrigin, const size_t* hostOrigin, const size_t* region, size_t bufferRowPitch, size_t bufferSlicePitch, size_t hostRowPitch, size_t hostSlicePitch, const void* ptr, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN cl_int enqueueCopyInto(CommandQueue* commandQueue, Buffer* destination, size_t srcOffset, size_t dstOffset, size_t size, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN cl_int enqueueCopyIntoRect(CommandQueue* commandQueue, Buffer* destination, const size_t* srcOrigin, const size_t* dstOrigin, const size_t* region, size_t srcRowPitch, size_t srcSlicePitch, size_t dstRowPitch, size_t dstSlicePitch, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN cl_int enqueueFill(CommandQueue* commandQueue, const void* pattern, size_t patternSize, size_t offset, size_t size, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN void* enqueueMap(CommandQueue* commandQueue, bool blockingMap, cl_map_flags mapFlags, size_t offset, size_t size, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event, cl_int* errcode_ret);
		CHECK_RETURN cl_int setDestructorCallback(BufferCallback callback, void* userData);
		CHECK_RETURN cl_int enqueueUnmap(CommandQueue* commandQueue, void* mappedPtr, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
		CHECK_RETURN virtual cl_int getInfo(cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

		void setUseHostPointer(void* hostPtr, size_t hostSize);
		void setAllocateHostPointer(size_t hostSize);
		void setCopyHostPointer(void* hostPtr, size_t hostSize);
		cl_mem_flags getMemFlags() const;

		std::list<void*> mappings;

		bool readable;
		bool writeable;
		bool hostReadable;
		bool hostWriteable;

		std::shared_ptr<DeviceBuffer> deviceBuffer;

	protected:
		bool useHostPtr;
		bool allocHostPtr;
		bool copyHostPtr;

		void* hostPtr = nullptr;
		size_t hostSize = 0;

		std::vector<std::pair<BufferCallback, void*>> callbacks;

		object_wrapper<Buffer> parent;
		size_t offset = 0;

		CHECK_RETURN Event* createBufferActionEvent(CommandQueue* queue, CommandType command_type, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_int* errcode_ret) const;

		friend class Image;
		friend class BufferMapping;
	};

	struct BufferMapping : public EventAction
	{
		object_wrapper<Buffer> buffer;
		void* hostPtr;
		bool unmap;

		BufferMapping(Buffer* buffer, void* hostPtr, bool unmap);

		cl_int operator()(Event* event) override;
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

		cl_int operator()(Event* event) override;
	};

	struct BufferRectAccess : public BufferAccess
	{
		std::array<std::size_t,3> region;
		std::array<std::size_t,3> bufferOrigin;
		std::size_t bufferRowPitch;
		std::size_t bufferSlicePitch;
		std::array<std::size_t,3> hostOrigin;
		std::size_t hostRowPitch;
		std::size_t hostSlicePitch;

		BufferRectAccess(Buffer* buffer, void* hostPtr, const std::size_t region[3], bool writeBuffer);

		cl_int operator()(Event* event) override;
	};
	
	struct BufferFill : public EventAction
	{
		object_wrapper<Buffer> buffer;
		std::size_t bufferOffset;
		std::vector<char> pattern;
		std::size_t numBytes;

		BufferFill(Buffer* buffer, const void* pattern, std::size_t patternSize, std::size_t numBytes);

		cl_int operator()(Event* event) override;
	};

	struct BufferCopy : public EventAction
	{
		object_wrapper<Buffer> sourceBuffer;
		std::size_t sourceOffset;
		object_wrapper<Buffer> destBuffer;
		std::size_t destOffset;
		std::size_t numBytes;

		BufferCopy(Buffer* src, Buffer* dest, std::size_t numBytes);

		cl_int operator()(Event* event) override;
	};

	struct BufferRectCopy : public EventAction
	{
		object_wrapper<Buffer> sourceBuffer;
		object_wrapper<Buffer> destBuffer;
		std::array<std::size_t,3> region;
		std::array<std::size_t,3> sourceOrigin;
		std::size_t sourceRowPitch;
		std::size_t sourceSlicePitch;
		std::array<std::size_t,3> destOrigin;
		std::size_t destRowPitch;
		std::size_t destSlicePitch;

		BufferRectCopy(Buffer* src, Buffer* dest, const std::size_t region[3]);

		cl_int operator()(Event* event) override;
	};

} /* namespace vc4cl */

#endif /* VC4CL_BUFFER */
