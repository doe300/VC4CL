/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "SVM.h"
#include "Event.h"
#include "extensions.h"
#include "V3D.h"

#include <unordered_map>

using namespace vc4cl;

//TODO create buffer from SVM pointer -> don't create new buffer (if use_host_pr is set)

//Since the SVM is always only returned as void*, but we need to keep the GPU-RAM alive (and free it in the end), we keep a list of all SVM objects
static std::unordered_map<const void*, SharedVirtualMemory> allocatedSVMs;

SharedVirtualMemory::SharedVirtualMemory(Context* context, std::shared_ptr<DeviceBuffer> buffer) : HasContext(context), buffer(buffer)
{

}

SharedVirtualMemory::~SharedVirtualMemory()
{

}

cl_int SharedVirtualMemory::getHostOffset(const void* hostPointer) const
{
	if(hostPointer < buffer->hostPointer)
		return CL_INVALID_VALUE;
	if(((intptr_t)hostPointer) >= ((intptr_t)buffer->hostPointer) + buffer->size)
		return CL_INVALID_VALUE;
	return ((intptr_t)hostPointer) - ((intptr_t)buffer->hostPointer);
}

void* SharedVirtualMemory::getDevicePointer(const size_t offset)
{
	return buffer->qpuPointer + offset;
}

void* SharedVirtualMemory::getHostPointer(const size_t offset)
{
	return buffer->hostPointer + offset;
}

SharedVirtualMemory* SharedVirtualMemory::findSVM(const void* hostPtr)
{
	//1. check match to start of SVM buffer
	if(allocatedSVMs.find(hostPtr) != allocatedSVMs.end())
		return &allocatedSVMs.at(hostPtr);
	//2. check match within SVM buffer
	//This is required for clSetKernelArgSVMPointer, where "The SVM pointer value specified as the argument value can be the pointer returned by clSVMAlloc or can be a pointer + offset into the SVM region."
	for(auto& pair : allocatedSVMs)
	{
		if(pair.second.buffer->hostPointer <= hostPtr && pair.second.buffer->hostPointer + pair.second.buffer->size > hostPtr)
			return &pair.second;
	}
	return nullptr;
}

/*!
 * OpenCL 2.0 specification, pages 167+:
 *  Allocates a shared virtual memory buffer (referred to as a SVM buffer) that can be shared by the host and all devices in an OpenCL context that support shared virtual memory.
 *
 *  \param context is a valid OpenCL context used to create the SVM buffer.
 *
 *  \param flags is a bit-field that is used to specify allocation and usage information. Table 5.14 describes the possible values for flags.
 *  If CL_MEM_SVM_FINE_GRAIN_BUFFER is not specified, the buffer can be created as a coarse grained SVM allocation.  Similarly, if CL_MEM_SVM_ATOMICS is not specified, the buffer can be created without support for the OpenCL 2.0 SVM atomic operations
 *  (refer to section 6.13.11 of the OpenCL C 2.0 specification).
 *
 *  \param size is the size in bytes of the SVM buffer to be allocated.
 *
 *  \param alignment is the minimum alignment in bytes that is required for the newly created buffer’s memory region.  It must be a power of two up to the largest data type supported by the OpenCL device.
 *  For the full profile, the largest data type is long16. For the embedded profile, it is long16 if the device supports 64-bit integers; otherwise it is int16.
 *  If alignment is 0, a default alignment will be used that is equal to the size of largest data type supported by the OpenCL implementation.
 *
 *  \return clSVMAlloc returns a valid non-NULL shared virtual memory address if the SVM buffer is successfully allocated. Otherwise, like malloc, it returns a NULL pointer value.
 *  clSVMAlloc will fail if
 *  - context is not a valid context.
 *  - flags does not contain CL_MEM_SVM_FINE_GRAIN_BUFFER but does contain CL_MEM_SVM_ATOMICS.
 *  - Values specified in flags do not follow rules described for supported values in table 5.14.
 *  - CL_MEM_SVM_FINE_GRAIN_BUFFER or CL_MEM_SVM_ATOMICS is specified in flags and these are not supported by at least one device in context.
 *  - The values specified in flags are not valid i.e. don’t match those defined in table 5.14.
 *  - size is 0 or > CL_DEVICE_MAX_MEM_ALLOC_SIZE value for any device in context.
 *  - alignment is not a power of two or the OpenCL implementation cannot support the specified alignment for at least one device in context.
 *  - There was a failure to allocate resources.
 *
 *  Calling clSVMAlloc does not itself provide consistency for the shared memory region. When the host can’t use the SVM atomic operations, it must rely on OpenCL’s guaranteed memory
 *  consistency at synchronization points. To initialize a buffer to be shared with a kernel, the host can create the buffer and use the resulting virtual memory pointer to initialize the buffer’s contents.
 *  For SVM to be used efficiently, the host and any devices sharing a buffer containing virtual memory pointers should have the same endianness. If the context passed to clSVMAlloc has devices with mixed endianness
 *  and the OpenCL implementation is unable to implement SVM because of that mixed endianness, clSVMAlloc will fail and return NULL.
 *  Although SVM is generally not supported for image objects, clCreateImage may create an image from a buffer (a 1D image from a buffer or a 2D image from buffer) if the buffer specified
 *  in its image description parameter is a SVM buffer. Such images have a linear memory representation so their memory can be shared using SVM. However, fine grained sharing and atomics are not supported for image reads and writes in a kernel.
 *
 *  If clCreateBuffer is called with a pointer returned by clSVMAlloc as its host_ptr argument, and CL_MEM_USE_HOST_PTR is set in its flags argument, clCreateBuffer will succeed and return a valid non-zero buffer object as long as the
 *  size argument to clCreateBuffer is no larger than the size argument passed in the original clSVMAlloc call. The new buffer object returned has the shared memory as the underlying storage.
 *  Locations in the buffer’s underlying shared memory can be operated on using, e.g., atomic operations if the device supports them.
 */
void* VC4CL_FUNC(clSVMAllocARM)(cl_context context, cl_svm_mem_flags_arm flags, size_t size, cl_uint alignment)
{
	cl_int error;
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), &error, void*);

	if((flags & CL_MEM_SVM_FINE_GRAIN_BUFFER_ARM) == 0 && (flags & CL_MEM_SVM_ATOMICS_ARM) != 0)
		return nullptr;
	//check for more than one of the following flags defined: CL_MEM_READ_WRITE, CL_MEM_WRITE_ONLY, CL_MEM_READ_ONLY
	if(((flags & CL_MEM_WRITE_ONLY) != 0) + ((flags & CL_MEM_READ_ONLY) != 0) + ((flags & CL_MEM_READ_WRITE) != 0) > 1)
		return nullptr;
	if((flags & CL_MEM_SVM_ATOMICS_ARM) != 0)
		//SVM atomic operations are not supported
		return nullptr;
	if(size == 0 || size > mailbox().getTotalGPUMemory())
		return nullptr;
	if(alignment % 2 != 0 || alignment < sizeof(cl_int16))
		return nullptr;

	std::shared_ptr<DeviceBuffer> buffer(mailbox().allocateBuffer(size, alignment));
	if(buffer.get() == nullptr)
		return nullptr;

	allocatedSVMs.emplace(buffer->hostPointer, SharedVirtualMemory(toType<Context>(context), buffer));
	return buffer->hostPointer;
}

/*!
 * OpenCL 2.0 specification, page 171:
 *  Frees a shared virtual memory buffer allocated using clSVMAlloc.
 *
 *  \param context is a valid OpenCL context used to create the SVM buffer.
 *
 *  \param svm_pointer must be the value returned by a call to clSVMAlloc.  If a NULL pointer is passed in svm_pointer, no action occurs.
 *
 *  Note that clSVMFree does not wait for previously enqueued commands that may be using svm_pointer to finish before freeing svm_pointer.
 *  It is the responsibility of the application to make sure that enqueued commands that use svm_pointer have finished before freeing svm_pointer.
 *  This can be done by enqueuing a blocking operation such as clFinish, clWaitForEvents, clEnqueueReadBuffer or by registering a callback with the events associated with enqueued commands and when the last
 *  enqueued command has finished freeing svm_pointer.
 *
 *  The behavior of using svm_pointer after it has been freed is undefined.  In addition, if a buffer object is created using clCreateBuffer with svm_pointer, the buffer object must first be released
 *  before the svm_pointer is freed. The clEnqueueSVMFree API  can also be used to enqueue a callback to free the shared virtual memory buffer allocated using clSVMAlloc or a shared system memory pointer.
 */
void VC4CL_FUNC(clSVMFreeARM)(cl_context context, void* svm_pointer)
{
	if(context == nullptr || toType<Context>(context) == nullptr || !toType<Context>(context)->checkReferences())
		return;
	if(svm_pointer == nullptr || allocatedSVMs.find(svm_pointer) == allocatedSVMs.end())
		return;
	//the deallocation of the SVM object frees the GPU memory (if it is not used by e.g. a buffer-object)
	allocatedSVMs.erase(svm_pointer);
}

/*!
 * OpenCL 2.0 specification, pages 172+:
 *  Enqueues a command to free the shared virtual memory allocated using clSVMAllocor a shared system memory pointer.
 *
 *  \param command_queue is a valid host command-queue.
 *
 *  \param svm_pointers and num_svm_pointers specify shared virtual memory pointers to be freed. Each pointer in svm_pointers that was allocated using clSVMAlloc must have been allocated from the same context from which
 *  command_queue was created. The memory associated with svm_pointers can be reused or freed after the function returns.
 *
 *  \param pfn_free_func specifies the callback function to be called to free the SVM pointers. pfn_free_func takes four arguments:
 *  queue which is the command queue in which clEnqueueSVMFree was enqueued, the count and list of SVM pointers to free and user_data which is a pointer to user specified data.
 *  If pfn_free_func is NULL, all pointers specified in svm_pointers must be allocated using clSVMAlloc and the OpenCL implementation will free these SVM pointers.
 *  pfn_free_func must be a valid callback function if any SVM pointer to be freed is a shared system memory pointer i.e. not allocated using clSVMAlloc.
 *  If pfn_free_func is a valid callback function, the OpenCL implementation will call pfn_free_func to free all the SVM pointers specified in svm_pointers.
 *
 *  \param user_data will be passed as the user_data argument when pfn_free_func is called. user_data can be NULL.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before clEnqueueSVMFree can be executed.  If event_wait_list is NULL, then clEnqueueSVMFree does not wait on any event to complete.
 *  If event_wait_list is NULL, num_events_in_wait_list must be 0. If event_wait_list is not NULL, the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0.
 *  The events specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and command_queue must be the same.
 *  The memory associated with event_wait_list can be reused or freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a wait for this particular command to complete.
 *  event can be NULL in which case it will not be possible for the application to query the status of this command or queue a wait for this command to complete.
 *  If the event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueSVMFree returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.
 *  - CL_INVALID_VALUE if num_svm_pointers is 0 or if svm_pointers is NULL or if any of the pointers specified in svm_pointers array is NULL.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clEnqueueSVMFreeARM)(cl_command_queue command_queue, cl_uint num_svm_pointers, void* svm_pointers[], void (CL_CALLBACK* pfn_free_func)(cl_command_queue, cl_uint, void*[], void*), void* user_data, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)
	if(num_svm_pointers == 0 || svm_pointers == nullptr)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Invalid (number of) SVM pointers specified");
	std::vector<void*> svmPointers;
	svmPointers.reserve(num_svm_pointers);
	for(cl_uint i = 0; i < num_svm_pointers; ++i)
	{
		if(svm_pointers[i] == nullptr)
			return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "One of the SVM pointers is NULL");
		svmPointers.push_back(svm_pointers[i]);
	}

	CommandQueue* queue = toType<CommandQueue>(command_queue);
	Event* e = newObject<Event>(queue->context(), CL_QUEUED, CommandType::SVM_FREE);
	CHECK_ALLOCATION(e)
	CustomSource* source = newObject<CustomSource>();
	CHECK_ALLOCATION(source)
	source->func = [svmPointers, pfn_free_func, user_data](Event* ev) -> cl_int
	{
		if(pfn_free_func != nullptr)
			pfn_free_func(ev->getCommandQueue()->toBase(), svmPointers.size(), const_cast<void**>(svmPointers.data()), user_data);
		//the deallocation of the SVM object frees the GPU memory (if it is not used by e.g. a buffer-object)
		for(void* ptr : svmPointers)
			allocatedSVMs.erase(ptr);
		return CL_SUCCESS;
	};
	e->source.reset(source);

	if(event != NULL)
		*event = e->toBase();

	e->setEventWaitList(num_events_in_wait_list, event_wait_list);
	cl_int status = queue->enqueueEvent(e);
	if(status != CL_SUCCESS)
		return returnError(status, __FILE__, __LINE__, "Enqueuing free SVM failed!");
	return CL_SUCCESS;
}

/*!
 * OpenCL 2.0 specification, pages 173+:
 *  The following function enqueues a command to do a memcpy operation.
 *
 *  \param command_queue refers to the host command-queue in which the read / write command will be queued.
 *  If either dst_ptr or src_ptr is allocated using clSVMAlloc then the OpenCL context allocated against must match that of command_queue.
 *
 *  \param blocking_copy indicates if the copy operation is blocking or non-blocking. If blocking_copy is CL_TRUE i.e. the copy command is blocking, clEnqueueSVMMemcpy does not return until the buffer data has been copied into memory pointed to by
 *  dst_ptr. If blocking_copy is CL_FALSE i.e. the copy command is non-blocking, clEnqueueSVMMemcpy queues a non-blocking copy command and returns.
 *  The contents of the buffer that dst_ptr point to cannot be used until the copy command has completed.  The event argument returns an event object which can be used to query the execution status of the read command.
 *  When the copy command has completed, the contents of the buffer that dst_ptr points to can be used by the application.
 *
 *  \param size is the size in bytes of data being copied.
 *
 *  \param dst_ptr is the pointer to a memory region where data is copied to.
 *
 *  \param src_ptr is the pointer to a memory region where data is copied from.
 *
 *  If dst_ptr and/or src_ptr are allocated using clSVMAlloc and either is not allocated from the same context from which command_queue was created the behavior is undefined.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular command can be executed.  If event_wait_list is NULL, then this particular command does not wait on any event to complete.
 *  If event_wait_list is NULL, num_events_in_wait_list must be 0.  If event_wait_list is not NULL, the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0.
 *  The events specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and command_queue must be the same.  The memory associated with event_wait_list can be reused or freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular read / write command and can be used to query or queue a wait for this particular command to complete.
 *  event can be NULL in which case it will not be possible for the application to query the status of this command or queue a wait for this command to complete.
 *  If the event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueSVMMemcpy returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the copy operation is blocking and the execution status of any of the events in event_wait_list is a negative integer value.
 *  - CL_INVALID_VALUE if dst_ptr or src_ptr are NULL.
 *  - CL_INVALID_VALUE if size is 0.
 *  - CL_MEM_COPY_OVERLAP if the values specified for dst_ptr, src_ptr and size result in an overlapping copy.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clEnqueueSVMMemcpyARM)(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr, const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)
	if(src_ptr == nullptr || dst_ptr == nullptr || size == 0)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Cannot copy from/to NULL pointer or with a size of zero");
	if(((uintptr_t)src_ptr) <= ((uintptr_t)dst_ptr) && (((uintptr_t)src_ptr) + size) > ((uintptr_t)dst_ptr))
		return returnError(CL_MEM_COPY_OVERLAP, __FILE__, __LINE__, "Source and destination areas overlay");
	if(((uintptr_t)dst_ptr) <= ((uintptr_t)src_ptr) && (((uintptr_t)dst_ptr) + size) > ((uintptr_t)src_ptr))
		return returnError(CL_MEM_COPY_OVERLAP, __FILE__, __LINE__, "Source and destination areas overlay");

	CommandQueue* commandQueue = toType<CommandQueue>(command_queue);
	for(cl_uint i = 0; i < num_events_in_wait_list; ++i)
	{
		Event* e = toType<Event>(event_wait_list[i]);
		if(e == nullptr || !e->checkReferences())
			return returnError(CL_INVALID_EVENT_WAIT_LIST, __FILE__, __LINE__, "Invalid event in wait-list");
		if(e->context() != commandQueue->context())
			return returnError(CL_INVALID_CONTEXT, __FILE__, __LINE__, "Contexts of event in wait-list and command-queue do not match");
	}

	Event* e = newObject<Event>(toType<CommandQueue>(command_queue)->context(), CL_QUEUED, CommandType::SVM_MEMCPY);
	CHECK_ALLOCATION(e)
	BufferSource* source = newObject<BufferSource>(const_cast<void*>(src_ptr), dst_ptr);
	CHECK_ALLOCATION(source)
	source->source.size = size;
	e->source.reset(source);

	if(event != NULL)
		*event = e->toBase();

	e->setEventWaitList(num_events_in_wait_list, event_wait_list);
	cl_int status = commandQueue->enqueueEvent(e);
	if(status != CL_SUCCESS)
		return returnError(status, __FILE__, __LINE__, "Enqueuing memcpy SVM failed!");
	if(blocking_copy)
	{
		return e->waitFor();
	}
	return CL_SUCCESS;
}

/*!
 * OpenCL 2.0 specification, pages 175+:
 *  Enqueues a command to fill a region in memory with a pattern of a given pattern size.
 *
 *  \param command_queue refers to the host command-queue in which the fill command will be queued.
 *  The OpenCL context associated with command_queue and SVM pointer referred to by svm_ptr must be the same.
 *
 *  \param svm_ptr is a pointer to a memory region that will be filled with pattern.  It must be aligned to pattern_size bytes.
 *  If svm_ptr is allocated using clSVMAlloc then it must be allocated from the same context from which command_queue was created. Otherwise the behavior is undefined.
 *
 *  \param pattern is a pointer to the data pattern of size pattern_size in bytes.
 *  pattern will be used to fill a region in buffer starting at svm_ptr and is size bytes in size.  The data pattern must be a scalar or vector integer or floating-point data type supported by OpenCL as described in
 *  sections 6.1.1 and 6.1.2. [...] The maximum value of pattern_size is the size of the largest integer or floating-point vector data type supported by the OpenCL device.
 *  The memory associated with pattern can be reused or freed after the function returns.
 *
 *  \param size is the size in bytes of region being filled starting with svm_ptr and must be a multiple of pattern_size.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular command can be executed.  If event_wait_list is NULL , then this particular command does not wait on any event to complete.
 *  If event_wait_list is NULL, num_events_in_wait_list must be 0.  If event_wait_list is not NULL, the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0.
 *  The events specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and command_queue must be the same. The memory associated with event_wait_list
 *  can be reused or freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a wait for this particular command to complete.
 *  event can be NULL in which case it will not be possible for the application to query the status of this command or queue a wait for this command to complete. clEnqueueBarrierWithWaitListcan be used instead.
 *  If the event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueSVMMemFill returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_VALUE if svm_ptr is NULL.
 *  - CL_INVALID_VALUE if svm_ptr is not aligned to pattern_size bytes.
 *  - CL_INVALID_VALUE if pattern is NULL or if pattern_size is 0 or if pattern_size is not one of {1, 2, 4, 8, 16, 32, 64, 128}.
 *  - CL_INVALID_VALUE if size is 0 or is not a multiple of pattern_size.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clEnqueueSVMMemFillARM)(cl_command_queue command_queue, void* svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint	num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)
	if(svm_ptr == nullptr)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "SVM pointer is NULL");
	if(((intptr_t)svm_ptr) % pattern_size != 0)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("SVM pointer is not aligned to %d", pattern_size));
	if(pattern == nullptr)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Pattern pointer is NULL");
	if(pattern_size == 0 || pattern_size % 2 != 0 || pattern_size > 128)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Pattern size is invalid: %d", pattern_size));
	if(size % pattern_size != 0)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Size %d is not a multiple of pattern-size %d", size, pattern_size));

	CommandQueue* commandQueue = toType<CommandQueue>(command_queue);
	for(cl_uint i = 0; i < num_events_in_wait_list; ++i)
	{
		Event* e = toType<Event>(event_wait_list[i]);
		if(e == nullptr || !e->checkReferences())
			return returnError(CL_INVALID_EVENT_WAIT_LIST, __FILE__, __LINE__, "Invalid event in wait-list");
		if(e->context() != commandQueue->context())
			return returnError(CL_INVALID_CONTEXT, __FILE__, __LINE__, "Contexts of event in wait-list and command-queue do not match");
	}

	Event* e = newObject<Event>(toType<CommandQueue>(command_queue)->context(), CL_QUEUED, CommandType::SVM_MEMFILL);
	CHECK_ALLOCATION(e)
	BufferSource* source = newObject<BufferSource>(static_cast<void*>(nullptr), svm_ptr);
	CHECK_ALLOCATION(source)
	source->source.size = size;
	source->source.setPattern(pattern, pattern_size);
	e->source.reset(source);

	if(event != NULL)
		*event = e->toBase();

	e->setEventWaitList(num_events_in_wait_list, event_wait_list);
	cl_int status = commandQueue->enqueueEvent(e);
	if(status != CL_SUCCESS)
		return returnError(status, __FILE__, __LINE__, "Enqueuing memfill SVM failed!");
	return CL_SUCCESS;
}

/*!
 * OpenCL 2.0 specification, pages 177+:
 *  Enqueues a command that will allow the host to update a region of a SVM buffer. Note that since we are enqueuing a command with a SVM buffer, the region is already mapped in the host a ddress space.
 *
 *  \param command_queue must be a valid host command-queue.
 *
 *  \param blocking_map indicates if the map operation is blocking or non-blocking. If blocking_map is CL_TRUE, clEnqueueSVMMap does not return until the application can access the contents of the SVM region specified by
 *  svm_ptr and size on the host. If blocking_map is CL_FALSE i.e. map operation is non-blocking, the region specified by svm_ptr and size cannot be used until the map command has completed.
 *  The event argument returns an event object which can be used to query the execution status of the map command. When the map command is completed,
 *  the application can access the contents of the region specified by svm_ptr and size.
 *
 *  \param map_flags is a bit-field and is described in table 5.5.
 *
 *  \param svm_ptr and size are a pointer to a memory region and size in bytes that will be updated by the host.
 *  If svm_ptr is allocated using clSVMAlloc then it must be allocated from the same context from which command_queue was created. Otherwise the behavior is undefined.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular command can be executed.  If event_wait_list is NULL, then this particular command does not wait on any event to complete.
 *  If event_wait_list is NULL, num_events_in_wait_list must be 0.  If event_wait_list is not NULL, the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0.
 *  The events specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and command_queue must be the same.  The memory associated with event_wait_list can be reused or freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a wait for this particular command to complete.
 *  event can be NULL in which case it will not be possible for the application to query the status of this command or queue a wait for this command to complete. clEnqueueBarrierWithWaitList can be used instead.
 *  If the event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueSVMMap returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_VALUE if svm_ptr is NULL.
 *  - CL_INVALID_VALUE if size is 0 or if values specified in map_flags are not valid.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the map operation is blocking and the execution status of any of the events in event_wait_list is a negative integer value.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clEnqueueSVMMapARM)(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags map_flags, void* svm_ptr, size_t size, cl_uint	num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)
	if(svm_ptr == nullptr || size == 0)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Cannot map a NULL SVM pointer or a pointer of size zero");
	if(((map_flags & CL_MAP_READ) == CL_MAP_READ) + ((map_flags & CL_MAP_WRITE) == CL_MAP_WRITE) + ((map_flags & CL_MAP_WRITE_INVALIDATE_REGION) == CL_MAP_WRITE_INVALIDATE_REGION) > 1)
		//the possible map flags are mutually exclusive
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Invalid map flags specified");

	CommandQueue* commandQueue = toType<CommandQueue>(command_queue);
	for(cl_uint i = 0; i < num_events_in_wait_list; ++i)
	{
		Event* e = toType<Event>(event_wait_list[i]);
		if(e == nullptr || !e->checkReferences())
			return returnError(CL_INVALID_EVENT_WAIT_LIST, __FILE__, __LINE__, "Invalid event in wait-list");
		if(e->context() != commandQueue->context())
			return returnError(CL_INVALID_CONTEXT, __FILE__, __LINE__, "Contexts of event in wait-list and command-queue do not match");
	}

	Event* e = newObject<Event>(toType<CommandQueue>(command_queue)->context(), CL_QUEUED, CommandType::SVM_MAP);
	CHECK_ALLOCATION(e)

	if(event != NULL)
		*event = e->toBase();

	e->setEventWaitList(num_events_in_wait_list, event_wait_list);
	cl_int status = commandQueue->enqueueEvent(e);
	if(status != CL_SUCCESS)
		return returnError(status, __FILE__, __LINE__, "Enqueuing map SVM failed!");
	return CL_SUCCESS;
}

/*!
 * OpenCL 2.0 specification, pages 178+:
 *  Enqueues a command to indicate that the host has completed updating the region given by svm_ptr and which was specified in a previous call to clEnqueueSVMMap.
 *
 *  \param command_queue must be a valid host command-queue.
 *
 *  \param svm_ptr is a pointer that was specified in a previous call to clEnqueueSVMMap. If svm_ptr is allocated using clSVMAlloc then it must be allocated from the same context from which command_queue was created.
 *  Otherwise the behavior is undefined.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before clEnqueueSVMUnmap can be executed.  If event_wait_list is NULL, then clEnqueueUnmap does not wait on any event to complete.
 *  If event_wait_list is NULL, num_events_in_wait_list must be 0.  If event_wait_list is not NULL, the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0.
 *  The events specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and command_queue must be the same.
 *  The memory associated with event_wait_list can be reused or freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a wait for this particular command to complete.
 *  event can be NULL in which case it will not be possible for the application to query the status of this command or queue a wait for this command to complete. clEnqueueBarrierWithWaitList can be used instead.
 *  If the event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueSVMUnmap returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid host command-queue.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_VALUE if svm_ptr is NULL.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or if event_wait_list is not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *  clEnqueueSVMMap, and clEnqueueSVMUnmap act as synchronization points for the region of the SVM buffer specified in these calls.
 *
 *  NOTE:
 *  If a coarse-grained SVM buffer is currently mapped for writing, the application must ensure that the SVM buffer is unmapped before any enqueued kernels or commands that read from or write to this SVM buffer or any of its associated cl_mem buffer objects
 *  begin execution; otherwise the behavior is undefined. If a coarse-grained SVM buffer is currently mapped for reading, the application must ensure that the SVM buffer is unmapped before any enqueued kernels or commands that write to this
 *  memory object or any of its associated cl_mem buffer objects begin execution; otherwise the behavior is undefined.
 *  A SVM buffer is considered as mapped if there are one or more active mappings for the SVM buffer irrespective of whether the mapped regions span the entire SVM buffer.
 *  The above note does not apply to fine-grained SVM buffers (fine-grained buffers allocated using clSVMAlloc or fine-grained system allocations).
 */
cl_int VC4CL_FUNC(clEnqueueSVMUnmapARM)(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)
	if(svm_ptr == nullptr)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "SVM pointer is NULL");

	CommandQueue* commandQueue = toType<CommandQueue>(command_queue);
	for(cl_uint i = 0; i < num_events_in_wait_list; ++i)
	{
		Event* e = toType<Event>(event_wait_list[i]);
		if(e == nullptr || !e->checkReferences())
			return returnError(CL_INVALID_EVENT_WAIT_LIST, __FILE__, __LINE__, "Invalid event in wait-list");
		if(e->context() != commandQueue->context())
			return returnError(CL_INVALID_CONTEXT, __FILE__, __LINE__, "Contexts of event in wait-list and command-queue do not match");
	}

	Event* e = newObject<Event>(toType<CommandQueue>(command_queue)->context(), CL_QUEUED, CommandType::SVM_UNMAP);
	CHECK_ALLOCATION(e)

	if(event != NULL)
		*event = e->toBase();

	e->setEventWaitList(num_events_in_wait_list, event_wait_list);
	cl_int status = commandQueue->enqueueEvent(e);
	if(status != CL_SUCCESS)
		return returnError(status, __FILE__, __LINE__, "Enqueuing unmap SVM failed!");
	return CL_SUCCESS;
}
