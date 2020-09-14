/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#include "Buffer.h"

using namespace vc4cl;

Buffer::Buffer(Context* context, cl_mem_flags flags) :
    HasContext(context), readable(true), writeable(true), hostReadable(true), hostWriteable(true), parent(nullptr)
{
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_WRITE_ONLY))
        readable = false;
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_READ_ONLY))
        writeable = false;
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_READ_ONLY) || hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_NO_ACCESS))
        hostWriteable = false;
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_WRITE_ONLY) || hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_NO_ACCESS))
        hostReadable = false;

    useHostPtr = hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR);
    allocHostPtr = hasFlag<cl_mem_flags>(flags, CL_MEM_ALLOC_HOST_PTR);
    copyHostPtr = hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR);
}

Buffer::Buffer(Buffer* parent, cl_mem_flags flags) : Buffer(parent->context(), flags)
{
    this->parent.reset(parent);
}

Buffer::~Buffer()
{
    // fire callbacks
    // "The registered user callback functions are called in the reverse order in which they were registered."
    for(auto it = callbacks.rbegin(); it != callbacks.rend(); ++it)
    {
        it->first(this->toBase(), it->second);
    }
}

Buffer* Buffer::createSubBuffer(
    cl_mem_flags flags, cl_buffer_create_type buffer_create_type, const void* buffer_create_info, cl_int* errcode_ret)
{
    if(parent)
        return returnError<Buffer*>(
            CL_INVALID_MEM_OBJECT, errcode_ret, __FILE__, __LINE__, "Parent is not a valid buffer!");

    if(!readable && (hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE) || hasFlag<cl_mem_flags>(flags, CL_MEM_READ_ONLY)))
        return returnError<Buffer*>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Readable flag contradicts parent!");
    if(!writeable &&
        (hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE) || hasFlag<cl_mem_flags>(flags, CL_MEM_WRITE_ONLY)))
        return returnError<Buffer*>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Writable flag contradicts parent!");
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_ALLOC_HOST_PTR) || hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR) ||
        hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR))
        return returnError<Buffer*>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Invalid host flags for sub-buffer!");

    if(!hostReadable && hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_READ_ONLY))
        return returnError<Buffer*>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Host readability flag contradicts parent!");
    if(!hostWriteable && hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_WRITE_ONLY))
        return returnError<Buffer*>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Host writability flag contradicts parent!");

    const cl_buffer_region* region;
    switch(buffer_create_type)
    {
    case CL_BUFFER_CREATE_TYPE_REGION:
        if(static_cast<const cl_buffer_region*>(buffer_create_info)->size == 0)
            return returnError<Buffer*>(
                CL_INVALID_BUFFER_SIZE, errcode_ret, __FILE__, __LINE__, "Sub buffer has no size!");
        region = static_cast<const cl_buffer_region*>(buffer_create_info);
        break;
    default:
        return returnError<Buffer*>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
            buildString("Invalid/Unsupported cl_buffer_create_type %u!", buffer_create_type));
    }

    if(region != nullptr)
    {
        if(region->size == 0)
            return returnError<Buffer*>(
                CL_INVALID_BUFFER_SIZE, errcode_ret, __FILE__, __LINE__, "Sub buffer has no size!");
        if(hostPtr != nullptr && region->origin + region->size > hostSize)
            return returnError<Buffer*>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
                buildString(
                    "Sub buffer maximum (%u) exceeds parent's (%u) size!", region->origin + region->size, hostSize));
        if(region->origin % device_config::BUFFER_ALIGNMENT != 0)
            return returnError<Buffer*>(CL_MISALIGNED_SUB_BUFFER_OFFSET, errcode_ret, __FILE__, __LINE__,
                "Sub-buffer has invalid alignment of!");
    }

    Buffer* subBuffer = newOpenCLObject<Buffer>(this, flags);
    CHECK_ALLOCATION_ERROR_CODE(subBuffer, errcode_ret, Buffer*)

    // set default flags from parent
    if(!(hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE) || hasFlag<cl_mem_flags>(flags, CL_MEM_READ_ONLY) ||
           hasFlag<cl_mem_flags>(flags, CL_MEM_WRITE_ONLY)))
    {
        subBuffer->readable = readable;
        subBuffer->writeable = writeable;
    }
    if(!(hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_READ_ONLY) || hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_WRITE_ONLY) ||
           hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_NO_ACCESS)))
    {
        subBuffer->hostReadable = hostReadable;
        subBuffer->hostWriteable = hostWriteable;
    }
    subBuffer->useHostPtr = useHostPtr;
    subBuffer->allocHostPtr = allocHostPtr;
    subBuffer->copyHostPtr = copyHostPtr;

    // set sub-region
    if(region != nullptr)
    {
        if(hostPtr != nullptr)
        {
            subBuffer->hostPtr = reinterpret_cast<uint8_t*>(hostPtr) + region->origin;
        }
        subBuffer->hostSize = region->size;
        subBuffer->subBufferOffset = region->origin;
        if(deviceBuffer->memHandle != 0)
        {
            subBuffer->deviceBuffer = deviceBuffer;
        }
    }
    // TODO if region is nullptr, sub-buffer has no device buffer. On purpose? Can this happen?
    subBuffer->setHostSize();

    return subBuffer;
}

cl_int Buffer::enqueueRead(CommandQueue* commandQueue, bool blockingRead, size_t offset, size_t size, void* ptr,
    cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event)
{
    if(size == 0 || offset + size > hostSize || ptr == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid read size (%u)!", size));
    if(!hostReadable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Can't read from non host-readable buffer!");

    cl_int errcode = CL_SUCCESS;
    Event* e = createBufferActionEvent(commandQueue, CommandType::BUFFER_READ, numEventsInWaitList, waitList, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create buffer event!");
    }

    BufferAccess* access = newObject<BufferAccess>(this, ptr, size, false);
    CHECK_ALLOCATION(access)
    access->bufferOffset = offset;

    e->action.reset(access);
    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);

    if(errcode == CL_SUCCESS && blockingRead)
        errcode = e->waitFor();

    return e->setAsResultOrRelease(errcode, event);
}

cl_int Buffer::enqueueWrite(CommandQueue* commandQueue, bool blockingWrite, size_t offset, size_t size, const void* ptr,
    cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event)
{
    if(size == 0 || offset + size > hostSize || ptr == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid write size (%u)!", size));
    if(!hostWriteable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot write to non-writeable buffer");

    cl_int errcode = CL_SUCCESS;
    Event* e =
        createBufferActionEvent(commandQueue, CommandType::BUFFER_WRITE, numEventsInWaitList, waitList, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create buffer event!");
    }

    BufferAccess* access = newObject<BufferAccess>(this, const_cast<void*>(ptr), size, true);
    CHECK_ALLOCATION(access)
    access->bufferOffset = offset;
    e->action.reset(access);

    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);

    if(errcode == CL_SUCCESS && blockingWrite)
        errcode = e->waitFor();

    return e->setAsResultOrRelease(errcode, event);
}

static size_t calculate_offset(const size_t* origin, size_t row_pitch, size_t slice_pitch)
{
    // as specified in OpenCL 1.2 specification, page 82
    return origin[2] * slice_pitch + origin[1] * row_pitch + origin[0];
}

/*
 * NOTE: The value returned here is wrong for rectangular access, but can server as quick in-bounds check
 */
static size_t calculate_size_bounds(const size_t* region)
{
    return region[0] /* width (in bytes) */ * region[1] /* height (in rows) */ * region[2] /* depth (in slices) */;
}

cl_int Buffer::enqueueReadRect(CommandQueue* commandQueue, bool blocking_read, const size_t* buffer_origin,
    const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch,
    size_t host_row_pitch, size_t host_slice_pitch, void* ptr, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    // only used for range-checks
    const size_t buffer_offset = calculate_offset(buffer_origin, buffer_row_pitch, buffer_slice_pitch);
    const size_t size = calculate_size_bounds(region);

    if(size == 0 || buffer_offset + size > hostSize)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid read size (%u)!", size));
    if(ptr == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Read destination pointer in NULL");

    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    if(!hostReadable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot read from non-readable buffer!");

    cl_int errcode = CL_SUCCESS;
    Event* e = createBufferActionEvent(
        commandQueue, CommandType::BUFFER_READ_RECT, num_events_in_wait_list, event_wait_list, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create buffer event!");
    }

    BufferRectAccess* access = newObject<BufferRectAccess>(this, ptr, region, false);
    CHECK_ALLOCATION(access)
    // the offset inside the (sub-buffer) is handled via the 3-dimensional rectangle
    access->bufferOffset = 0;
    memcpy(access->bufferOrigin.data(), buffer_origin, 3 * sizeof(size_t));
    access->bufferRowPitch = buffer_row_pitch;
    access->bufferSlicePitch = buffer_slice_pitch;
    memcpy(access->hostOrigin.data(), host_origin, 3 * sizeof(size_t));
    access->hostRowPitch = host_row_pitch;
    access->hostSlicePitch = host_slice_pitch;
    e->action.reset(access);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    errcode = commandQueue->enqueueEvent(e);

    if(errcode == CL_SUCCESS && blocking_read)
        errcode = e->waitFor();

    return e->setAsResultOrRelease(errcode, event);
}

cl_int Buffer::enqueueWriteRect(CommandQueue* commandQueue, bool blocking_write, const size_t* buffer_origin,
    const size_t* host_origin, const size_t* region, size_t buffer_row_pitch, size_t buffer_slice_pitch,
    size_t host_row_pitch, size_t host_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    // only used for range-checks
    const size_t buffer_offset = calculate_offset(buffer_origin, buffer_row_pitch, buffer_slice_pitch);
    const size_t size = calculate_size_bounds(region);

    if(size == 0 || buffer_offset + size > hostSize || ptr == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid write size (%u)!", size));
    if(!hostWriteable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot write to non-writeable buffer");

    cl_int errcode = CL_SUCCESS;
    Event* e = createBufferActionEvent(
        commandQueue, CommandType::BUFFER_WRITE_RECT, num_events_in_wait_list, event_wait_list, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create buffer event!");
    }

    BufferRectAccess* access = newObject<BufferRectAccess>(this, const_cast<void*>(ptr), region, true);
    CHECK_ALLOCATION(access)
    // the offset inside the (sub-buffer) is handled via the 3-dimensional rectangle
    access->bufferOffset = 0;
    memcpy(access->bufferOrigin.data(), buffer_origin, 3 * sizeof(size_t));
    access->bufferRowPitch = buffer_row_pitch;
    access->bufferSlicePitch = buffer_slice_pitch;
    memcpy(access->hostOrigin.data(), host_origin, 3 * sizeof(size_t));
    access->hostRowPitch = host_row_pitch;
    access->hostSlicePitch = host_slice_pitch;
    e->action.reset(access);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    errcode = commandQueue->enqueueEvent(e);

    if(errcode == CL_SUCCESS && blocking_write)
        errcode = e->waitFor();

    return e->setAsResultOrRelease(errcode, event);
}

cl_int Buffer::enqueueCopyInto(CommandQueue* commandQueue, Buffer* destination, size_t src_offset, size_t dst_offset,
    size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    if(size == 0 || src_offset + size > hostSize || dst_offset + size > destination->hostSize)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid copy size (%u)!", size));

    if(this == destination || (parent && destination->parent && parent.get() == destination->parent.get()))
    {
        /*
         * The buffers refer to the same (parent) buffer object, check for actual overlap:
         *
         * "[Returns] CL_MEM_COPY_OVERLAP if src_buffer and dst_buffer are the same buffer or sub-buffer object and the
         * source and destination regions overlap or if src_buffer and dst_buffer are different sub-buffers of the same
         * associated buffer object and they overlap. The regions overlap if src_offset <= dst_offset <= src_offset +
         * size – 1 or if dst_offset <= src_offset <= dst_offset + size – 1."
         */
        auto srcBase = reinterpret_cast<uintptr_t>(getDeviceHostPointerWithOffset()) + src_offset;
        auto destBase = reinterpret_cast<uintptr_t>(destination->getDeviceHostPointerWithOffset()) + dst_offset;
        if(srcBase <= destBase && destBase <= (srcBase + size - 1))
            return returnError(CL_MEM_COPY_OVERLAP, __FILE__, __LINE__, "Source and destination buffers overlap!");
        if(destBase <= srcBase && srcBase <= (destBase + size - 1))
            return returnError(CL_MEM_COPY_OVERLAP, __FILE__, __LINE__, "Source and destination buffers overlap!");
    }

    cl_int errcode = CL_SUCCESS;
    Event* e = createBufferActionEvent(
        commandQueue, CommandType::BUFFER_COPY, num_events_in_wait_list, event_wait_list, &errcode);
    if(e == nullptr)
    {
        return errcode;
    }

    // set source and destination
    BufferCopy* action = newObject<BufferCopy>(this, destination, size);
    CHECK_ALLOCATION(action)
    action->sourceOffset = src_offset;
    action->destOffset = dst_offset;
    e->action.reset(action);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    errcode = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

/**
 * The following check function is adapted from the OpenCL 1.2 specification, Appendix E and is provided under the
 * following license:
 *
 * Copyright (c) 2011 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and /or associated
 * documentation files (the "Materials "), to deal in the Materials without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to the following conditions: The above copyright
 * notice and this permission notice shall be included in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE MATERIALS OR THE USE OR OTHER DEALINGS IN THE
 * MATERIALS.
 */
static bool check_copy_overlap(const size_t src_offset[3], const size_t dst_offset[3], const size_t region[3],
    size_t row_pitch, size_t slice_pitch)
{
    const size_t src_min[] = {src_offset[0], src_offset[1], src_offset[2]};
    const size_t src_max[] = {src_offset[0] + region[0], src_offset[1] + region[1], src_offset[2] + region[2]};
    const size_t dst_min[] = {dst_offset[0], dst_offset[1], dst_offset[2]};
    const size_t dst_max[] = {dst_offset[0] + region[0], dst_offset[1] + region[1], dst_offset[2] + region[2]};
    // Check for overlap
    bool overlap = true;
    unsigned i;
    for(i = 0; i != 3; ++i)
    {
        overlap = overlap && (src_min[i] < dst_max[i]) && (src_max[i] > dst_min[i]);
    }
    size_t dst_start = dst_offset[2] * slice_pitch + dst_offset[1] * row_pitch + dst_offset[0];
    size_t dst_end = dst_start + (region[2] * slice_pitch + region[1] * row_pitch + region[0]);
    size_t src_start = src_offset[2] * slice_pitch + src_offset[1] * row_pitch + src_offset[0];
    size_t src_end = src_start + (region[2] * slice_pitch + region[1] * row_pitch + region[0]);
    if(!overlap)
    {
        size_t delta_src_x = (src_offset[0] + region[0] > row_pitch) ? src_offset[0] + region[0] - row_pitch : 0;
        size_t delta_dst_x = (dst_offset[0] + region[0] > row_pitch) ? dst_offset[0] + region[0] - row_pitch : 0;
        if((delta_src_x > 0 && delta_src_x > dst_offset[0]) || (delta_dst_x > 0 && delta_dst_x > src_offset[0]))
        {
            if((src_start <= dst_start && dst_start < src_end) || (dst_start <= src_start && src_start < dst_end))
                overlap = true;
        }
        if(region[2] > 1)
        {
            size_t src_height = slice_pitch / row_pitch;
            size_t dst_height = slice_pitch / row_pitch;
            size_t delta_src_y = (src_offset[1] + region[1] > src_height) ? src_offset[1] + region[1] - src_height : 0;
            size_t delta_dst_y = (dst_offset[1] + region[1] > dst_height) ? dst_offset[1] + region[1] - dst_height : 0;
            if((delta_src_y > 0 && delta_src_y > dst_offset[1]) || (delta_dst_y > 0 && delta_dst_y > src_offset[1]))
            {
                if((src_start <= dst_start && dst_start < src_end) || (dst_start <= src_start && src_start < dst_end))
                    overlap = true;
            }
        }
    }
    return overlap;
}

cl_int Buffer::enqueueCopyIntoRect(CommandQueue* commandQueue, Buffer* destination, const size_t* src_origin,
    const size_t* dst_origin, const size_t* region, size_t src_row_pitch, size_t src_slice_pitch, size_t dst_row_pitch,
    size_t dst_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    // only used for range-checks
    const size_t src_offset = calculate_offset(src_origin, src_row_pitch, src_slice_pitch);
    const size_t size = calculate_size_bounds(region);
    const size_t dst_offset = calculate_offset(dst_origin, dst_row_pitch, dst_slice_pitch);

    if(size == 0 || src_offset + size > hostSize || dst_offset + size > destination->hostSize)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid copy size (%u)!", size));
    if(src_row_pitch == 0)
        // "If src_row_pitch is 0, src_row_pitch is computed as region[0]."
        src_row_pitch = region[0];
    if(src_slice_pitch == 0)
        // "If src_slice_pitch is 0, src_slice_pitch is computed as region[1] * src_row_pitch.""
        src_slice_pitch = region[1] * src_row_pitch;
    if(dst_row_pitch == 0)
        // "If dst_row_pitch is 0, dst_row_pitch is computed as region[0]."
        dst_row_pitch = region[0];
    if(dst_slice_pitch == 0)
        // "If dst_slice_pitch is 0, dst_slice_pitch is computed as region[1] * dst_row_pitch."
        dst_slice_pitch = region[1] * dst_row_pitch;
    if(src_row_pitch < region[0] || (src_slice_pitch < region[1] * src_row_pitch) ||
        (src_slice_pitch % src_row_pitch != 0))
        // "CL_INVALID_VALUE if src_row_pitch is not 0 and is less than region[0]. [...] if src_slice_pitch is not 0 and
        // is less than region[1] * src_row_pitch or if src_slice_pitch is not 0 and is not a multiple of
        // src_row_pitch."
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
            buildString("Invalid source pitches (%u and %u)!", src_row_pitch, src_slice_pitch));
    if(dst_row_pitch < region[0] || (dst_slice_pitch < region[1] * dst_row_pitch) ||
        (dst_slice_pitch % dst_row_pitch != 0))
        // "CL_INVALID_VALUE if dst_row_pitch is not 0 and is less than region[0]. [...] if dst_slice_pitch is not 0 and
        // is less than region[1] * dst_row_pitch or if dst_slice_pitch is not 0 and is not a multiple of dst_row_pitch.
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
            buildString("Invalid destination pitches (%u and %u)!", dst_row_pitch, dst_slice_pitch));
    if(this == destination && src_slice_pitch != dst_slice_pitch)
        // "If src_buffer and dst_buffer are the same buffer object, src_row_pitch must equal dst_row_pitch and
        // src_slice_pitch must equal dst_slice_pitch."
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
            buildString("Slice pitches must match for copying from/to the same buffer (%u and %u)!", src_slice_pitch,
                dst_slice_pitch));
    if(this == destination && src_row_pitch != dst_row_pitch)
        // "If src_buffer and dst_buffer are the same buffer object, src_row_pitch must equal dst_row_pitch and
        // src_slice_pitch must equal dst_slice_pitch."
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
            buildString("Row pitches must match for copying from/to the same buffer (%u and %u)!", src_row_pitch,
                dst_row_pitch));
    if(this == destination)
    {
        /*
         * The buffers refer to the same (parent) buffer object, check for actual overlap:
         *
         * "[Returns] CL_MEM_COPY_OVERLAP if src_buffer and dst_buffer are the same buffer or sub-buffer object and the
         * source and destination regions overlap or if src_buffer and dst_buffer are different sub-buffers of the same
         * associated buffer object and they overlap. Refer to Appendix E for details on how to determine if source and
         * destination regions overlap."
         */
        // TODO does not handle subbuffers of same parent. Also specification does not say anything about subbuffer +
        // parent buffer!
        if(check_copy_overlap(src_origin, dst_origin, region, src_row_pitch, src_slice_pitch))
            return returnError(CL_MEM_COPY_OVERLAP, __FILE__, __LINE__, "Source and destination regions overlap!");
    }

    cl_int errcode = CL_SUCCESS;
    Event* e = createBufferActionEvent(
        commandQueue, CommandType::BUFFER_COPY_RECT, num_events_in_wait_list, event_wait_list, &errcode);
    if(e == nullptr)
    {
        return errcode;
    }

    // set source and destination
    BufferRectCopy* action = newObject<BufferRectCopy>(this, destination, region);
    CHECK_ALLOCATION(action)
    memcpy(action->sourceOrigin.data(), src_origin, 3 * sizeof(size_t));
    action->sourceRowPitch = src_row_pitch;
    action->sourceSlicePitch = src_slice_pitch;
    memcpy(action->destOrigin.data(), dst_origin, 3 * sizeof(size_t));
    action->destRowPitch = dst_row_pitch;
    action->destSlicePitch = dst_slice_pitch;
    e->action.reset(action);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    errcode = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

cl_int Buffer::enqueueFill(CommandQueue* commandQueue, const void* pattern, size_t pattern_size, size_t offset,
    size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    if(size == 0 || offset + size > hostSize)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid fill size (%u)", size));
    if(pattern == nullptr || pattern_size == 0 || offset % pattern_size != 0)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
            buildString("Invalid pattern %p or pattern-size %u", pattern, pattern_size));
    if(!hostWriteable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot fill a non host-writeable buffer!");

    cl_int errcode = CL_SUCCESS;
    Event* e = createBufferActionEvent(
        commandQueue, CommandType::BUFFER_FILL, num_events_in_wait_list, event_wait_list, &errcode);
    if(e == nullptr)
    {
        return errcode;
    }

    // set source and destination
    BufferFill* action = newObject<BufferFill>(this, pattern, pattern_size, size);
    CHECK_ALLOCATION(action)
    action->bufferOffset = offset;
    e->action.reset(action);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    errcode = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

void* Buffer::enqueueMap(CommandQueue* commandQueue, bool blocking_map, cl_map_flags map_flags, size_t offset,
    size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret)
{
    if(commandQueue->context() != context())
        return returnError<void*>(
            CL_INVALID_CONTEXT, errcode_ret, __FILE__, __LINE__, "Contexts of command queue and buffer do not match!");

    if(size == 0 || offset + size > hostSize)
        return returnError<void*>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
            buildString("Maximum position (%u) exceeds buffer-size (%u)!", offset + size, hostSize));

    if(!hostReadable && hasFlag<cl_map_flags>(map_flags, CL_MAP_READ))
        return returnError<void*>(
            CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Cannot read from not host-readable buffer!");
    if(!hostWriteable &&
        (hasFlag<cl_map_flags>(map_flags, CL_MAP_WRITE) ||
            hasFlag<cl_map_flags>(map_flags, CL_MAP_WRITE_INVALIDATE_REGION)))
        return returnError<void*>(
            CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Cannot write to not host-writeable buffer!");

    // mapping = making the buffer available in the host-memory
    // our implementation does so automatically

    Event* e = createBufferActionEvent(
        commandQueue, CommandType::BUFFER_MAP, num_events_in_wait_list, event_wait_list, errcode_ret);
    if(e == nullptr)
    {
        return nullptr;
    }

    uintptr_t out_ptr = reinterpret_cast<uintptr_t>(nullptr);
    //"If the buffer object is created with CL_MEM_USE_HOST_PTR [...]"
    if(useHostPtr && hostPtr != nullptr)
    {
        //"The pointer value returned by clEnqueueMapBuffer will be derived from the host_ptr specified when the buffer
        // object is created."
        out_ptr = reinterpret_cast<uintptr_t>(hostPtr) + offset;
    }
    else
    {
        out_ptr = reinterpret_cast<uintptr_t>(getDeviceHostPointerWithOffset()) + offset;
    }

    std::list<MappingInfo>::const_iterator info = mappings.end();
    {
        // we need to already add the mapping here, otherwise queuing the clEnqueueUnmapMemObject might fail for the
        // memory are not being mapped yet, if the event handler did not process this event yet.
        std::lock_guard<std::mutex> mapGuard(mappingsLock);
        mappings.emplace_back(MappingInfo{reinterpret_cast<void*>(out_ptr), false,
            hasFlag<cl_map_flags>(map_flags, CL_MAP_WRITE_INVALIDATE_REGION),
            /* only on direct match, i.e. if not combined with CL_MAP_WRITE(...) */
            map_flags == CL_MAP_READ});
        info = --mappings.end();
    }

    EventAction* action = newObject<BufferMapping>(this, info, false);
    CHECK_ALLOCATION_ERROR_CODE(action, errcode_ret, void*)
    e->action.reset(action);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    cl_int status = commandQueue->enqueueEvent(e);

    if(status == CL_SUCCESS && blocking_map)
        status = e->waitFor();

    status = e->setAsResultOrRelease(status, event);
    if(status != CL_SUCCESS)
        return returnError<void*>(status, errcode_ret, __FILE__, __LINE__, "Error releasing the event object!");
    RETURN_OBJECT(reinterpret_cast<void*>(out_ptr), errcode_ret)
}

cl_int Buffer::setDestructorCallback(BufferDestructionCallback callback, void* userData)
{
    if(callback == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Cannot set a NULL callback!");

    callbacks.emplace_back(callback, userData);
    return CL_SUCCESS;
}

cl_int Buffer::enqueueUnmap(CommandQueue* commandQueue, void* mapped_ptr, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    std::list<MappingInfo>::iterator mapping = mappings.end();
    {
        std::lock_guard<std::mutex> mapGuard(mappingsLock);
        if(mapped_ptr == nullptr)
            return returnError(
                CL_INVALID_VALUE, __FILE__, __LINE__, buildString("No such memory area to unmap %p!", mapped_ptr));

        for(auto it = mappings.begin(); it != mappings.end(); ++it)
        {
            if(it->hostPointer == mapped_ptr && !it->unmapScheduled)
            {
                mapping = it;
                break;
            }
        }
        if(mapping == mappings.end())
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
                buildString("Memory area %p was not mapped to this buffer!", mapped_ptr));
        else
            // mark this particular entry as being unmapped to make sure we do not unmap an entry twice
            mapping->unmapScheduled = true;
    }

    cl_int errcode = CL_SUCCESS;
    Event* e = createBufferActionEvent(
        commandQueue, CommandType::BUFFER_UNMAP, num_events_in_wait_list, event_wait_list, &errcode);
    if(e == nullptr)
    {
        return errcode;
    }

    EventAction* action = newObject<BufferMapping>(this, mapping, true);
    CHECK_ALLOCATION(action)
    e->action.reset(action);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    cl_int status = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(status, event);
}

cl_int Buffer::getInfo(cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    switch(param_name)
    {
    case CL_MEM_TYPE:
        return returnValue<cl_mem_object_type>(
            CL_MEM_OBJECT_BUFFER, param_value_size, param_value, param_value_size_ret);
    case CL_MEM_FLAGS:
        return returnValue<cl_mem_flags>(getMemFlags(), param_value_size, param_value, param_value_size_ret);
    case CL_MEM_SIZE:
        //"Return actual size of the data store associated with memobj in bytes. "
        return returnValue<size_t>(hostSize, param_value_size, param_value, param_value_size_ret);
    case CL_MEM_HOST_PTR:
        if(useHostPtr)
            return returnValue<void*>(hostPtr, param_value_size, param_value, param_value_size_ret);
        return returnValue<void*>(nullptr, param_value_size, param_value, param_value_size_ret);
    case CL_MEM_MAP_COUNT:
    {
        std::lock_guard<std::mutex> mapGuard(mappingsLock);
        return returnValue<cl_uint>(
            static_cast<cl_uint>(mappings.size()), param_value_size, param_value, param_value_size_ret);
    }
    case CL_MEM_REFERENCE_COUNT:
        return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
    case CL_MEM_CONTEXT:
        return returnValue<cl_context>(context()->toBase(), param_value_size, param_value, param_value_size_ret);
    case CL_MEM_ASSOCIATED_MEMOBJECT:
        return returnValue<cl_mem>(
            parent ? parent->toBase() : nullptr, param_value_size, param_value, param_value_size_ret);
    case CL_MEM_OFFSET:
        if(parent)
            return returnValue<size_t>(subBufferOffset, param_value_size, param_value, param_value_size_ret);
        return returnValue<size_t>(0, param_value_size, param_value, param_value_size_ret);
    }

    return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_mem_info value %u", param_name));
}

void Buffer::setUseHostPointer(void* hostPtr, size_t hostSize)
{
    useHostPtr = true;
    this->hostPtr = hostPtr;
    this->hostSize = hostSize;
}

void Buffer::setAllocateHostPointer(size_t hostSize)
{
    allocHostPtr = true;
    this->hostPtr = deviceBuffer->hostPointer;
    this->hostSize = hostSize;
}

void Buffer::setCopyHostPointer(void* hostPtr, size_t hostSize)
{
    copyHostPtr = true;
    this->hostSize = hostSize;
    memmove(deviceBuffer->hostPointer, hostPtr, hostSize);
}

cl_mem_flags Buffer::getMemFlags() const
{
    return (readable && writeable ? CL_MEM_READ_WRITE : 0) | (readable && !writeable ? CL_MEM_READ_ONLY : 0) |
        (writeable && !readable ? CL_MEM_WRITE_ONLY : 0) |
        (hostReadable && !hostWriteable ? CL_MEM_HOST_READ_ONLY : 0) |
        (hostWriteable && !hostReadable ? CL_MEM_HOST_WRITE_ONLY : 0) |
        (!hostReadable && !hostWriteable ? CL_MEM_HOST_NO_ACCESS : 0) | (useHostPtr ? CL_MEM_USE_HOST_PTR : 0) |
        (allocHostPtr ? CL_MEM_ALLOC_HOST_PTR : 0) | (copyHostPtr ? CL_MEM_COPY_HOST_PTR : 0);
}

Event* Buffer::createBufferActionEvent(CommandQueue* commandQueue, CommandType command_type,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_int* errcode_ret) const
{
    CHECK_COMMAND_QUEUE_ERROR_CODE(commandQueue, errcode_ret, Event*)
    if(commandQueue->context() != context())
        return returnError<Event*>(
            CL_INVALID_CONTEXT, errcode_ret, __FILE__, __LINE__, "Contexts of command queue and buffer do not match!");
    CHECK_EVENT_WAIT_LIST_ERROR_CODE(event_wait_list, num_events_in_wait_list, errcode_ret, Event*)

    Event* event = newOpenCLObject<Event>(const_cast<Context*>(context()), CL_QUEUED, command_type);
    CHECK_ALLOCATION_ERROR_CODE(event, errcode_ret, Event*)
    RETURN_OBJECT(event, errcode_ret)
}

cl_int Buffer::copyIntoHostBuffer(size_t offset, size_t size)
{
    /*
     * Copies the data from the device-buffer into the host-buffer, if the buffer was created with CL_MEM_USE_HOST_PTR
     *
     * XXX when does this need to be called? After every kernel writing this buffer, after write-buffer, ...?
     * Intel Beignet as well as Intel compute-runtime (the successor of Beignet) copy the memory the same places VC4CL
     * does:
     * * Once the buffer is created with the CL_MEM_USE_HOST_PTR flag (host -> GPU)
     * * If memory created with the CL_MEM_USE_HOST_PTR flag is mapped (GPU -> host)
     * * if memory created with the CL_MEM_USE_HOST_PTR flag is unmapped (host -> GPU)
     */
    if(!useHostPtr)
        return CL_SUCCESS;
    if(hostPtr == nullptr)
        return CL_INVALID_VALUE;
    if(offset + size > hostSize)
        return CL_INVALID_VALUE;
    if(hostPtr == deviceBuffer->hostPointer)
        // e.g. allocate host-pointer
        return CL_SUCCESS;
    uintptr_t dest = reinterpret_cast<uintptr_t>(hostPtr) + offset;
    uintptr_t src = reinterpret_cast<uintptr_t>(getDeviceHostPointerWithOffset()) + offset;
    if(dest == src)
        return CL_SUCCESS;
    memmove(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(src), size);
    return CL_SUCCESS;
}

cl_int Buffer::copyFromHostBuffer(size_t offset, size_t size)
{
    /*
     * Copies the data from the host-buffer into the device-buffer, if the buffer was created with CL_MEM_USE_HOST_PTR
     */
    if(!useHostPtr)
        return CL_SUCCESS;
    if(hostPtr == nullptr)
        return CL_INVALID_VALUE;
    if(offset + size > hostSize)
        return CL_INVALID_VALUE;
    if(hostPtr == deviceBuffer->hostPointer)
        // e.g. allocate host-pointer
        return CL_SUCCESS;
    uintptr_t dest = reinterpret_cast<uintptr_t>(getDeviceHostPointerWithOffset()) + offset;
    uintptr_t src = reinterpret_cast<uintptr_t>(hostPtr) + offset;
    memmove(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(src), size);
    return CL_SUCCESS;
}

void Buffer::setHostSize()
{
    if(hostSize == 0)
        hostSize = deviceBuffer->size;
}

DevicePointer Buffer::getDevicePointerWithOffset()
{
    if(!deviceBuffer)
        return DevicePointer{0};
    return DevicePointer{static_cast<unsigned>(static_cast<unsigned>(deviceBuffer->qpuPointer) + subBufferOffset)};
}

void* Buffer::getDeviceHostPointerWithOffset()
{
    if(!deviceBuffer)
        return nullptr;
    auto tmp = reinterpret_cast<char*>(deviceBuffer->hostPointer) + subBufferOffset;
    return reinterpret_cast<void*>(tmp);
}

BufferMapping::BufferMapping(Buffer* buffer, std::list<MappingInfo>::const_iterator mappingInfo, bool unmap) :
    buffer(buffer), mappingInfo(mappingInfo), unmap(unmap)
{
}

BufferMapping::~BufferMapping() = default;

cl_int BufferMapping::operator()()
{
    /*
     * Despite the clEnqueueMapBuffer and clEnqueueUnmapMemObject functions being called with sub-areas of the buffer
     * (offset + size), we always copy the whole buffer from/to host memory.
     */
    cl_int status = CL_SUCCESS;
    if(unmap)
    {
        //"Reads or writes from the host using the pointer returned by clEnqueueMapBuffer or clEnqueueMapImage are
        // considered to be complete."
        //-> when un-mapping, we need to write possible changes back to the device buffer, unless the mapping was
        // read-only in which case writing to it would have been undefined behavior
        if(!mappingInfo->skipWritingBack)
            status = buffer->copyFromHostBuffer(0, buffer->hostSize);
        std::lock_guard<std::mutex> mapGuard(buffer->mappingsLock);
        buffer->mappings.erase(mappingInfo);
    }
    else
    {
        //"If the buffer object is created with CL_MEM_USE_HOST_PTR [...]"
        //"The host_ptr specified in clCreateBuffer is guaranteed to contain the latest bits [...]"
        // -> when mapping, we need to write the current device buffer contents to the host buffer, unless the
        // client notified us that it does not care about the previous contents, e.g. if the whole buffer will be
        // overwritten anyway
        if(!mappingInfo->skipPopulatingBuffer)
            status = buffer->copyIntoHostBuffer(0, buffer->hostSize);
    }
    return status;
}

BufferAccess::BufferAccess(Buffer* buffer, void* hostPtr, std::size_t numBytes, bool writeBuffer) :
    buffer(buffer), bufferOffset(0), hostPtr(hostPtr), hostOffset(0), numBytes(numBytes), writeToBuffer(writeBuffer)
{
}

BufferAccess::~BufferAccess() = default;

cl_int BufferAccess::operator()()
{
    if(hostPtr == buffer->getDeviceHostPointerWithOffset() && bufferOffset == hostOffset)
        return CL_SUCCESS;
    if(writeToBuffer)
        memmove(static_cast<char*>(buffer->getDeviceHostPointerWithOffset()) + bufferOffset,
            static_cast<char*>(hostPtr) + hostOffset, numBytes);
    else
        memmove(static_cast<char*>(hostPtr) + hostOffset,
            static_cast<char*>(buffer->getDeviceHostPointerWithOffset()) + bufferOffset, numBytes);
    return CL_SUCCESS;
}

BufferRectAccess::BufferRectAccess(Buffer* buf, void* hostPointer, const std::size_t region[3], bool writeBuffer) :
    BufferAccess(buf, hostPointer, region[0] * region[1] * region[2], writeBuffer), region{0, 0, 0}, bufferOrigin{0, 0,
                                                                                                         0},
    bufferRowPitch(0), bufferSlicePitch(0), hostOrigin{0, 0, 0}, hostRowPitch(0), hostSlicePitch(0)
{
    memcpy(this->region.data(), region, 3 * sizeof(size_t));
}

BufferRectAccess::~BufferRectAccess() = default;

cl_int BufferRectAccess::operator()()
{
    // copied from POCL (https://github.com/pocl/pocl/blob/master/lib/CL/devices/basic/basic.c), functions
    // pocl_basic_write_rect and pocl_basic_read_rect
    uintptr_t devicePointer = reinterpret_cast<uintptr_t>(buffer->getDeviceHostPointerWithOffset()) + bufferOrigin[0] +
        bufferOrigin[1] * bufferRowPitch + bufferOrigin[2] * bufferSlicePitch;
    uintptr_t hostPointer = reinterpret_cast<uintptr_t>(hostPtr) + hostOrigin[0] + hostOrigin[1] * hostRowPitch +
        hostOrigin[2] * hostSlicePitch;

    /* TODO: (from pocl) handle overlapping regions. Can there be any? */
    for(std::size_t z = 0; z < region[2]; ++z)
    {
        for(std::size_t y = 0; y < region[1]; ++y)
        {
            auto bufferOffsetPointer =
                reinterpret_cast<void*>(devicePointer + bufferRowPitch * y + bufferSlicePitch * z);
            auto hostOffsetPointer = reinterpret_cast<void*>(hostPointer + hostRowPitch * y + hostSlicePitch * z);
            if(writeToBuffer)
                memmove(bufferOffsetPointer, hostOffsetPointer, region[0]);
            else
                memmove(hostOffsetPointer, bufferOffsetPointer, region[0]);
        }
    }
    return CL_SUCCESS;
}

BufferFill::BufferFill(Buffer* buffer, const void* pattern, std::size_t patternSize, std::size_t numBytes) :
    buffer(buffer), bufferOffset(0), numBytes(numBytes)
{
    // OpenCL 1.2 specification, page 85: "The memory associated with pattern can be reused or freed after the function
    // returns."  so we need to copy the pattern
    this->pattern.resize(patternSize);
    memcpy(this->pattern.data(), pattern, patternSize);
}

BufferFill::~BufferFill() = default;

cl_int BufferFill::operator()()
{
    uintptr_t start = reinterpret_cast<uintptr_t>(buffer->getDeviceHostPointerWithOffset()) + bufferOffset;
    uintptr_t end = start + numBytes;
    while(start < end)
    {
        memcpy(reinterpret_cast<void*>(start), pattern.data(), pattern.size());
        start += pattern.size();
    }
    return CL_SUCCESS;
}

BufferCopy::BufferCopy(Buffer* src, Buffer* dest, std::size_t numBytes) :
    sourceBuffer(src), sourceOffset(0), destBuffer(dest), destOffset(0), numBytes(numBytes)
{
}

BufferCopy::~BufferCopy() = default;

cl_int BufferCopy::operator()()
{
    uintptr_t src = reinterpret_cast<uintptr_t>(sourceBuffer->getDeviceHostPointerWithOffset()) + sourceOffset;
    uintptr_t dest = reinterpret_cast<uintptr_t>(destBuffer->getDeviceHostPointerWithOffset()) + destOffset;
    if(dest == src)
        return CL_SUCCESS;
    memmove(reinterpret_cast<void*>(dest), reinterpret_cast<void*>(src), numBytes);
    return CL_SUCCESS;
}

BufferRectCopy::BufferRectCopy(Buffer* src, Buffer* dest, const std::size_t region[3]) :
    sourceBuffer(src), destBuffer(dest), region{0, 0, 0}, sourceOrigin{0, 0, 0}, sourceRowPitch(0),
    sourceSlicePitch(0), destOrigin{0, 0, 0}, destRowPitch(0), destSlicePitch(0)
{
    memcpy(this->region.data(), region, 3 * sizeof(size_t));
}

BufferRectCopy::~BufferRectCopy() = default;

cl_int BufferRectCopy::operator()()
{
    // copied from POCL (https://github.com/pocl/pocl/blob/master/lib/CL/devices/basic/basic.c), function
    // pocl_basic_copy_rect
    uintptr_t sourcePointer = reinterpret_cast<uintptr_t>(sourceBuffer->getDeviceHostPointerWithOffset()) +
        sourceOrigin[0] + sourceOrigin[1] * sourceRowPitch + sourceOrigin[2] * sourceSlicePitch;
    uintptr_t destPointer = reinterpret_cast<uintptr_t>(destBuffer->getDeviceHostPointerWithOffset()) + destOrigin[0] +
        destOrigin[1] * destRowPitch + destOrigin[2] * destSlicePitch;

    /* TODO: (from pocl) handle overlapping regions. Can there be any? */
    for(std::size_t z = 0; z < region[2]; ++z)
    {
        for(std::size_t y = 0; y < region[1]; ++y)
        {
            memmove(reinterpret_cast<void*>(destPointer + destRowPitch * y + destSlicePitch * z),
                reinterpret_cast<void*>(sourcePointer + sourceRowPitch * y + sourceSlicePitch * z), region[0]);
        }
    }
    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 67+:
 *  A buffer object is created using the following function
 *
 *  \param context is a valid OpenCL context used to create the buffer object.
 *
 *  \param flags is a bit-field that is used to specify allocation and usage information such as the memory arena that
 * should be used to allocate the buffer object and how it will be used. Table 5.3 describes the possible values for
 * flags.  If value specified for flags is 0, the default is used which is CL_MEM_READ_WRITE.
 *
 *  \param size is the size in bytes of the buffer memory object to be allocated.
 *
 *  \param host_ptr is a pointer to the buffer data that may already be allocated by the application.  The size of the
 * buffer that host_ptr points to must be >= size bytes.
 *
 *  \param errcode_ret will return an appropriate error code.  If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateBuffer returns a valid non-zero buffer object and errcode_ret is set to CL_SUCCESS if the buffer
 * object is created successfully. Otherwise, it returns a NULL value with one of the following error values returned in
 * errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if values specified in flags are not valid as defined in table 5.3.
 *  - CL_INVALID_BUFFER_SIZE if size is 0.
 *  - CL_INVALID_HOST_PTR if host_ptr is NULL and CL_MEM_USE_HOST_PTR or CL_MEM_COPY_HOST_PTR are set in flags or if
 * host_ptr is not NULL but CL_MEM_COPY_HOST_PTR or CL_MEM_USE_HOST_PTR are not set in flags.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for buffer object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_mem VC4CL_FUNC(clCreateBuffer)(
    cl_context context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_mem", clCreateBuffer, "cl_context", context, "cl_mem_flags", flags, "size_t", size,
        "void*", host_ptr, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_mem)

    //"If value specified for flags is 0, the default is used which is CL_MEM_READ_WRITE. "
    if(flags == 0)
        flags = CL_MEM_READ_WRITE;
    // if neither of CL_MEM_READ_WRITE, CL_MEM_READ_ONLY or CL_MEM_WRITE_ONLY is set, assume CL_MEM_READ_WRITE:
    //"This flag specifies that the memory object will be read and written by a kernel. This is the default."
    if(!hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE) && !hasFlag<cl_mem_flags>(flags, CL_MEM_READ_ONLY) &&
        !hasFlag<cl_mem_flags>(flags, CL_MEM_WRITE_ONLY))
        flags |= CL_MEM_READ_WRITE;

    if(moreThanOneMemoryAccessFlagSet(flags))
        return returnError<cl_mem>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "More than one memory-access flag set!");
    if(moreThanOneHostAccessFlagSet(flags))
        return returnError<cl_mem>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "More than one host-access flag set!");

    if(exceedsLimits<size_t>(size, 1, mailbox()->getTotalGPUMemory()))
        return returnError<cl_mem>(CL_INVALID_BUFFER_SIZE, errcode_ret, __FILE__, __LINE__,
            buildString("Buffer size (%u) exceeds system maximum (%u)!", size, mailbox()->getTotalGPUMemory()));

    if(host_ptr == nullptr &&
        (hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR) || hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR)))
        return returnError<cl_mem>(CL_INVALID_HOST_PTR, errcode_ret, __FILE__, __LINE__,
            "Usage of host-pointer specified in flags but no host-buffer given!");

    if(host_ptr != nullptr &&
        !(hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR) || hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR)))
        return returnError<cl_mem>(CL_INVALID_HOST_PTR, errcode_ret, __FILE__, __LINE__,
            "Host pointer given, but not used according to flags!");

    Buffer* buffer = newOpenCLObject<Buffer>(toType<Context>(context), flags);
    CHECK_ALLOCATION_ERROR_CODE(buffer, errcode_ret, cl_mem)

    buffer->deviceBuffer.reset(mailbox()->allocateBuffer(static_cast<unsigned>(size)));
    if(!buffer->deviceBuffer)
    {
        ignoreReturnValue(buffer->release(), __FILE__, __LINE__, "Already errored");
        return returnError<cl_mem>(CL_OUT_OF_RESOURCES, errcode_ret, __FILE__, __LINE__,
            buildString("Failed to allocate enough device memory (%u)!", size));
    }

    if(hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR))
    {
        //"OpenCL implementations are allowed to cache the buffer contents pointed to by host_ptr in device memory."
        buffer->setUseHostPointer(host_ptr, size);
        *errcode_ret = buffer->copyFromHostBuffer(0, size);
        if(*errcode_ret != CL_SUCCESS)
        {
            ignoreReturnValue(buffer->release(), __FILE__, __LINE__, "Already errored");
            return returnError<cl_mem>(
                *errcode_ret, errcode_ret, __FILE__, __LINE__, "Failed to copy host-memory into device-memory!");
        }
    }
    else if(hasFlag<cl_mem_flags>(flags, CL_MEM_ALLOC_HOST_PTR))
    {
        //"This flag specifies that the application wants the OpenCL implementation to allocate memory from host
        // accessible memory."
        //-> QPU memory is always host-accessible
        buffer->setAllocateHostPointer(size);
    }
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR))
    {
        if(host_ptr == nullptr)
        {
            ignoreReturnValue(buffer->release(), __FILE__, __LINE__, "Already errored");
            return returnError<cl_mem>(
                CL_INVALID_HOST_PTR, errcode_ret, __FILE__, __LINE__, "Cannot copy from NULL host-pointer!");
        }
        //"CL_MEM_COPY_HOST_PTR can be used with CL_MEM_ALLOC_HOST_PTR"
        buffer->setCopyHostPointer(host_ptr, size);
    }
    buffer->setHostSize();

    RETURN_OBJECT(buffer->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pages 70+:
 *  Can be used to create a new buffer object (referred to as a sub-buffer object) from an existing buffer object.
 *
 *  \param buffer must be a valid buffer object and cannot be a sub-buffer object.
 *
 *  \param flags is a bit-field that is used to specify allocation and usage information about the sub-buffer memory
 * object being created and is described in table 5.3. If the CL_MEM_READ_WRITE, CL_MEM_READ_ONLY or CL_MEM_WRITE_ONLY
 * values are not specified in flags, they are inherited from the corresponding memory access qualifers associated with
 * buffer. The CL_MEM_USE_HOST_PTR, CL_MEM_ALLOC_HOST_PTR and CL_MEM_COPY_HOST_PTR values cannot be specified in flags
 * but are inherited from the corresponding memory access qualifiers associated with buffer. If CL_MEM_COPY_HOST_PTR is
 * specified in  the memory access qualifier values associated with buffer it does not imply any additional copies when
 * the sub-buffer is created from buffer. If the CL_MEM_HOST_WRITE_ONLY, CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS
 * values are not specified in flags, they are inherited from the corresponding memory access qualifiers associated
 * with buffer.
 *
 *  \param buffer_create_type and buffer_create_info describe the type of buffer object to be created.  The list of
 * supported values for buffer_create_type and corresponding descriptor that buffer_create_info points to is described
 * in table 5.4.
 *
 *  \return clCreateSubBuffer returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one of
 * the following errors in errcode_ret:
 *  - CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object or is a sub-buffer object.
 *  - CL_INVALID_VALUE if buffer was created with CL_MEM_WRITE_ONLY and flags specifies CL_MEM_READ_WRITE or
 * CL_MEM_READ_ONLY, or if buffer was created with CL_MEM_READ_ONLY and flags specifies CL_MEM_READ_WRITE or
 * CL_MEM_WRITE_ONLY, or if flags specifies CL_MEM_USE_HOST_PTR or CL_MEM_ALLOC_HOST_PTR or CL_MEM_COPY_HOST_PTR.
 *  - CL_INVALID_VALUE if buffer was created with CL_MEM_HOST_WRITE_ONLY and flags specify CL_MEM_HOST_READ_ONLY, or if
 * buffer was created with CL_MEM_HOST_READ_ONLY and flags specify CL_MEM_HOST_WRITE_ONLY, or if buffer was created with
 * CL_MEM_HOST_NO_ACCESS and flags specify CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_WRITE_ONLY.
 *  - CL_INVALID_VALUE if value specified in buffer_create_type is not valid.
 *  - CL_INVALID_VALUE if value(s) specified in buffer_create_info (for a given buffer_create_type) is not valid or if
 * buffer_create_info is NULL.
 *  - CL_INVALID_BUFFER_SIZE if size is 0.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for sub-buffer object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_mem VC4CL_FUNC(clCreateSubBuffer)(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type,
    const void* buffer_create_info, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_mem", clCreateSubBuffer, "cl_mem", buffer, "cl_mem_flags", flags, "cl_buffer_create_type",
        buffer_create_type, "const void*", buffer_create_info, "cl_int*", errcode_ret);
    CHECK_BUFFER_ERROR_CODE(toType<Buffer>(buffer), errcode_ret, cl_mem)
    Buffer* subBuffer =
        toType<Buffer>(buffer)->createSubBuffer(flags, buffer_create_type, buffer_create_info, errcode_ret);
    CHECK_BUFFER_ERROR_CODE(subBuffer, errcode_ret, cl_mem)
    RETURN_OBJECT(subBuffer->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pages 73+:
 *  The following functions enqueue commands to read from a buffer object to host memory or write to a buffer object
 * from host memory.
 *
 *  \param command_queue refers to the command-queue in which the read / write command will be queued. command_queue and
 * buffer must be created with the same OpenCL context.
 *
 *  \param buffer refers to a valid buffer object.
 *
 *  \param blocking_read and blocking_write indicate if the read and write operations are blocking or non-blocking.
 *
 *  If blocking_read is CL_TRUE i.e. the read command is blocking, clEnqueueReadBuffer does not return until the buffer
 * data has been read and copied into memory pointed to by ptr. If blocking_read is CL_FALSE i.e. the read command is
 * non-blocking, clEnqueueReadBuffer queues a non-blocking read command and returns. The contents of the buffer that
 * ptr points to cannot be used until the read command has completed.  The event argument returns an event object which
 * can be used to query the execution status of the read command.  When the read command has completed, the contents of
 * the buffer that ptr points to can be used by the application.
 *
 *  \param offset is the offset in bytes in the buffer object to read from or write to.
 *
 *  \param size is the size in bytes of data being read or written.
 *
 *  \param ptr is the pointer to buffer in host memory where data is to be read into or to be written from.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL, then this particular command does not wait on any event to
 * complete.  If event_wait_list is NULL, num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular read / write command and can be used to query
 * or queue a wait for this particular command to complete. event can be NULL in which case it will not be possible for
 * the application to query the status of this command or queue await for this command to complete. If the
 * event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the
 * event_wait_list array.
 *
 *  \return clEnqueueReadBuffer and clEnqueueWriteBuffer return CL_SUCCESS if the function is executed successfully.
 * Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and buffer are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object.
 *  - CL_INVALID_VALUE if the region being read or written specified by (offset, size) is out of bounds or if ptr is a
 * NULL value or if size is 0.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object and offset specified when the sub-buffer object
 * is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the read and write operations are blocking and the execution
 * status of any of the events in event_wait_list is a negative integer value.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with buffer.
 *  - CL_INVALID_OPERATION if clEnqueueReadBuffer is called on buffer which has been created with CL_MEM_HOST_WRITE_ONLY
 * or CL_MEM_HOST_NO_ACCESS.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE:
 *  Calling clEnqueueReadBuffer to read a region of the buffer object with the ptr argument value set to host_ptr +
 * offset, where host_ptr is a pointer to the memory region specified when the buffer object being read is created with
 * CL_MEM_USE_HOST_PTR, must meet the following requirements in order to avoid undefined behavior:
 *  - All commands that use this buffer object or a memory object (buffer or image) created from this buffer object have
 * finished execution before the read command begins execution.
 *  - The buffer object or memory objects created from this buffer object are not mapped.
 *  - The buffer object or memory objects created from this buffer object are not used by any command-queue until the
 * read command has finished execution.
 */
cl_int VC4CL_FUNC(clEnqueueReadBuffer)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
    size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
    cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueReadBuffer, "cl_command_queue", command_queue, "cl_mem", buffer, "cl_bool",
        blocking_read, "size_t", offset, "size_t", size, "void*", ptr, "cl_uint", num_events_in_wait_list,
        "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(buffer))
    return toType<Buffer>(buffer)->enqueueRead(toType<CommandQueue>(command_queue), blocking_read == CL_TRUE, offset,
        size, ptr, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 73+:
 *  The following functions enqueue commands to read from a buffer object to host memory or write to a buffer object
 * from host memory.
 *
 *  \param command_queue refers to the command-queue in which the read / write command will be queued. command_queue and
 * buffer must be created with the same OpenCL context.
 *
 *  \param buffer refers to a valid buffer object.
 *
 *  \param blocking_read and blocking_write indicate if the read and write operations are blocking or non-blocking.
 *
 *  If blocking_write is CL_TRUE, the OpenCL implementation copies the data referred to by ptr and enqueues the write
 * operation in the command-queue. The memory pointed to by ptr can be reused by the application after the
 * clEnqueueWriteBuffer call returns. If blocking_write is CL_FALSE, the OpenCL implementation will use ptr to perform
 * a non-blocking write. As the write is non-blocking the implementation can return immediately. The memory pointed to
 * by ptr cannot be reused by the application after the call returns.  The event argument returns an event object which
 * can be used to query the execution status of the write command.  When the write command has completed, the memory
 * pointed to by ptr can then be reused by the application.
 *
 *  \param offset is the offset in bytes in the buffer object to read from or write to.
 *
 *  \param size is the size in bytes of data being read or written.
 *
 *  \param ptr is the pointer to buffer in host memory where data is to be read into or to be written from.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL, then this particular command does not wait on any event to
 * complete.  If event_wait_list is NULL, num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular read / write command and can be used to query
 * or queue a wait for this particular command to complete. event can be NULL in which case it will not be possible for
 * the application to query the status of this command or queue await for this command to complete. If the
 * event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the
 * event_wait_list array.
 *
 *  \return clEnqueueReadBuffer and clEnqueueWriteBuffer return CL_SUCCESS if the function is executed successfully.
 * Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and buffer are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object.
 *  - CL_INVALID_VALUE if the region being read or written specified by (offset, size) is out of bounds or if ptr is a
 * NULL value or if size is 0.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object and offset specified when the sub-buffer object
 * is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the read and write operations are blocking and the execution
 * status of any of the events in event_wait_list is a negative integer value.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with buffer.
 *  - CL_INVALID_OPERATION if clEnqueueWriteBuffer is called on buffer which has been created with CL_MEM_HOST_READ_ONLY
 * or CL_MEM_HOST_NO_ACCESS.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE:
 *  Calling clEnqueueWriteBuffer to update the latest bits in a region of the buffer object with the ptr argument value
 * set to host_ptr + offset, where host_ptr is a pointer to the memory region specified when the buffer object being
 * written is created with CL_MEM_USE_HOST_PTR, must meet the following requirements in order to avoid undefined
 * behavior:
 *  - The host memory region given by (host_ptr + offset, cb) contains the latest bits when the enqueued write command
 * begins execution.
 *  - The buffer object or memory objects created from this buffer object are not mapped.
 *  - The buffer object or memory objects created from this buffer object are not used by any command-queue until the
 * write command has finished execution.
 */
cl_int VC4CL_FUNC(clEnqueueWriteBuffer)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
    size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
    cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueWriteBuffer, "cl_command_queue", command_queue, "cl_mem", buffer, "cl_bool",
        blocking_write, "size_t", offset, "size_t", size, "void*", ptr, "cl_uint", num_events_in_wait_list,
        "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(buffer))

    return toType<Buffer>(buffer)->enqueueWrite(toType<CommandQueue>(command_queue), blocking_write == CL_TRUE, offset,
        size, ptr, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 75+:
 *  The following functions enqueue commands to read a 2D or 3D rectangular region from a buffer object to host memory
 * or write a 2D or 3D rectangular region to a buffer object from host memory.
 *
 *  \param command_queue refers to the command-queue in which the read / write command will be queued. command_queue and
 * buffer must be created with the same OpenCL context.
 *
 *  \param buffer refers to a valid buffer object.
 *
 *  \param blocking_read and blocking_write indicate if the read and write operations are blocking or non-blocking.
 *
 *  If blocking_read is CL_TRUE i.e. the read command is blocking, clEnqueueReadBufferRect does not return until the
 * buffer data has been read and copied into memory pointed to by ptr.
 *
 *  If blocking_read is CL_FALSE i.e. the read command is non-blocking, clEnqueueReadBufferRect queues a non-blocking
 * read command and returns. The contents of the buffer that ptr points to cannot be used until the read command has
 * completed.  The event argument returns an event object which can be used to query the execution status of the read
 * command.  When the read command has completed, the contents of the buffer that ptr points to can be used by the
 * application.
 *
 *  \param buffer_origin defines the (x, y, z) offset in the memory region associated with buffer. For a 2D rectangle
 * region, the z value given by buffer_origin[2] should be 0. The offset in bytes is computed as buffer_origin[2] *
 * buffer_slice_pitch + buffer_origin[1] * buffer_row_pitch + buffer_origin[0].
 *
 *  \param host_origin defines the (x, y, z) offset in the memory region pointed to by ptr. For a 2D rectangle region,
 * the z value given by host_origin[2] should be 0. The offset in bytes is computed as host_origin[2] * host_slice_pitch
 * + host_origin[1] * host_row_pitch + host_origin[0].
 *
 *  \param region defines the(width in bytes, height in rows, depth in slices) of the 2D or 3D rectangle being read or
 * written. For a 2D rectangle copy, the depth value given by region[2] should be 1. The values in region cannot be 0.
 *
 *  \param buffer_row_pitch is the length of each row in bytes to be used for the memory region associated with buffer.
 *  If buffer_row_pitch is 0, buffer_row_pitch is computed as region[0].
 *
 *  \param buffer_slice_pitch is the length of each 2D slice in bytes to be used for the memory region associated with
 * buffer. If buffer_slice_pitch is 0, buffer_slice_pitch is computed as region[1] * buffer_row_pitch.
 *
 *  \param host_row_pitch is the length of each row in bytes to be used for the memory region pointed to by ptr.
 *  If host_row_pitch is 0, host_row_pitch is computed as region[0].
 *
 *  \param host_slice_pitch is the length of each 2D slice in bytes to be used for the memory region pointed to by ptr.
 *  If host_slice_pitch is 0, host_slice_pitch is computed as region[1] * host_row_pitch.
 *
 *  \param ptr is the pointer to buffer in host memory where data is to be read into or to be written from.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL, then this particular command does not wait on any event to
 * complete.  If event_wait_list is NULL, num_events_in_wait_list must be 0. If event_wait_list is not NULL, the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and
 * command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function
 * returns.
 *
 *  \param event returns an event object that identifies this particular read / write command and can be used to query
 * or queue a wait for this particular command to complete. event can be NULL in which case it will not be possible for
 * the application to query the status of this command or queue await for this command to complete. If the
 * event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the
 * event_wait_list array.
 *
 *  \return clEnqueueReadBufferRect and clEnqueueWriteBufferRect return CL_SUCCESS if the function is executed
 * successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and buffer are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object.
 *  - CL_INVALID_VALUE if the region being read or written specified by (buffer_origin, region, buffer_row_pitch,
 * buffer_slice_pitch) is out of bounds.
 *  - CL_INVALID_VALUE if ptr is a NULL value.
 *  - CL_INVALID_VALUE if any region array element is 0.
 *  - CL_INVALID_VALUE if buffer_row_pitch is not 0 and is less than region[0].
 *  - CL_INVALID_VALUE if host_row_pitch is not 0 and is less than region[0].
 *  - CL_INVALID_VALUE if buffer_slice_pitch is not 0 and is less than region[1] * buffer_row_pitch and not a multiple
 * of buffer_row_pitch.
 *  - CL_INVALID_VALUE if host_slice_pitch is not 0 and is less than region[1] * host_row_pitch and not a multiple of
 * host_row_pitch.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_listis 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object and offset specified when the sub-buffer object
 * is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the read and write operations are blocking and the execution
 * status of any of the events in event_wait_list is a negative integer value.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with buffer.
 *  - CL_INVALID_OPERATION if clEnqueueReadBufferRect is called on buffer which has been created with
 * CL_MEM_HOST_WRITE_ONLY or CL_MEM_HOST_NO_ACCESS.
 *  - CL_INVALID_OPERATION if clEnqueueWriteBufferRect is called on buffer which has been created with
 * CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE:
 *  Calling clEnqueueReadBufferRect to read a region of the buffer object with the ptr argument value set to host_ptr
 * and host_origin, buffer_origin values are the same, where host_ptr is a pointer to the memory region specified when
 * the buffer object being read is created with CL_MEM_USE_HOST_PTR, must meet the same requirements given above for
 *  clEnqueueReadBuffer.
 */
cl_int VC4CL_FUNC(clEnqueueReadBufferRect)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
    const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch,
    size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueReadBufferRect, "cl_command_queue", command_queue, "cl_mem", buffer,
        "cl_bool", blocking_read, "const size_t*", buffer_origin, "const size_t*", host_origin, "const size_t*", region,
        "size_t", buffer_row_pitch, "size_t", buffer_slice_pitch, "size_t", host_row_pitch, "size_t", host_slice_pitch,
        "void*", ptr, "cl_uint", num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(buffer))
    return toType<Buffer>(buffer)->enqueueReadRect(toType<CommandQueue>(command_queue), blocking_read == CL_TRUE,
        buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr,
        num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 75+:
 *  The following functions enqueue commands to read a 2D or 3D rectangular region from a buffer object to host memory
 * or write a 2D or 3D rectangular region to a buffer object from host memory.
 *
 *  \param command_queue refers to the command-queue in which the read / write command will be queued. command_queue and
 * buffer must be created with the same OpenCL context.
 *
 *  \param buffer refers to a valid buffer object.
 *
 *  \param blocking_read and blocking_write indicate if the read and write operations are blocking or non-blocking.
 *
 *  If blocking_write is CL_TRUE, the OpenCL implementation copies the data referred to by ptr and enqueues the write
 * operation in the command-queue. The memory pointed to by ptr can be reused by the application after the
 * clEnqueueWriteBufferRect call returns.
 *
 *  If blocking_write is CL_FALSE, the OpenCL implementation will use ptr to perform a non-blocking write.  As the write
 * is non-blocking the implementation can return immediately. The memory pointed to by ptr cannot be reused by the
 * application after the call returns.  The event argument returns an event object which can be used to query the
 *  execution status of the write command.  When the write command has completed, the memory pointed to by ptr can then
 * be reused by the application.
 *
 *  \param buffer_origin defines the (x, y, z) offset in the memory region associated with buffer. For a 2D rectangle
 * region, the z value given by buffer_origin[2] should be 0. The offset in bytes is computed as buffer_origin[2] *
 * buffer_slice_pitch + buffer_origin[1] * buffer_row_pitch + buffer_origin[0].
 *
 *  \param host_origin defines the (x, y, z) offset in the memory region pointed to by ptr. For a 2D rectangle region,
 * the z value given by host_origin[2] should be 0. The offset in bytes is computed as host_origin[2] * host_slice_pitch
 * + host_origin[1] * host_row_pitch + host_origin[0].
 *
 *  \param region defines the(width in bytes, height in rows, depth in slices) of the 2D or 3D rectangle being read or
 * written. For a 2D rectangle copy, the depth value given by region[2] should be 1. The values in region cannot be 0.
 *
 *  \param buffer_row_pitch is the length of each row in bytes to be used for the memory region associated with buffer.
 *  If buffer_row_pitch is 0, buffer_row_pitch is computed as region[0].
 *
 *  \param buffer_slice_pitch is the length of each 2D slice in bytes to be used for the memory region associated with
 * buffer. If buffer_slice_pitch is 0, buffer_slice_pitch is computed as region[1] * buffer_row_pitch.
 *
 *  \param host_row_pitch is the length of each row in bytes to be used for the memory region pointed to by ptr.
 *  If host_row_pitch is 0, host_row_pitch is computed as region[0].
 *
 *  \param host_slice_pitch is the length of each 2D slice in bytes to be used for the memory region pointed to by ptr.
 *  If host_slice_pitch is 0, host_slice_pitch is computed as region[1] * host_row_pitch.
 *
 *  \param ptr is the pointer to buffer in host memory where data is to be read into or to be written from.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL, then this particular command does not wait on any event to
 * complete.  If event_wait_list is NULL, num_events_in_wait_list must be 0. If event_wait_list is not NULL, the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and
 * command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function
 * returns.
 *
 *  \param event returns an event object that identifies this particular read / write command and can be used to query
 * or queue a wait for this particular command to complete. event can be NULL in which case it will not be possible for
 * the application to query the status of this command or queue await for this command to complete. If the
 * event_wait_list and the event arguments are not NULL, the event argument should not refer to an element of the
 * event_wait_list array.
 *
 *  \return clEnqueueReadBufferRect and clEnqueueWriteBufferRect return CL_SUCCESS if the function is executed
 * successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and buffer are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object.
 *  - CL_INVALID_VALUE if the region being read or written specified by (buffer_origin, region, buffer_row_pitch,
 * buffer_slice_pitch) is out of bounds.
 *  - CL_INVALID_VALUE if ptr is a NULL value.
 *  - CL_INVALID_VALUE if any region array element is 0.
 *  - CL_INVALID_VALUE if buffer_row_pitch is not 0 and is less than region[0].
 *  - CL_INVALID_VALUE if host_row_pitch is not 0 and is less than region[0].
 *  - CL_INVALID_VALUE if buffer_slice_pitch is not 0 and is less than region[1] * buffer_row_pitch and not a multiple
 * of buffer_row_pitch.
 *  - CL_INVALID_VALUE if host_slice_pitch is not 0 and is less than region[1] * host_row_pitch and not a multiple of
 * host_row_pitch.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_listis 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object and offset specified when the sub-buffer object
 * is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the read and write operations are blocking and the execution
 * status of any of the events in event_wait_list is a negative integer value.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with buffer.
 *  - CL_INVALID_OPERATION if clEnqueueReadBufferRect is called on buffer which has been created with
 * CL_MEM_HOST_WRITE_ONLY or CL_MEM_HOST_NO_ACCESS.
 *  - CL_INVALID_OPERATION if clEnqueueWriteBufferRect is called on buffer which has been created with
 * CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE:
 *  Calling clEnqueueWriteBufferRect to update the latest bits in a region of the buffer object with the ptr argument
 * value set to host_ptr and host_origin, buffer_origin values are the same, where host_ptr is a pointer to the memory
 * region specified when the buffer object being written is created with CL_MEM_USE_HOST_PTR, must meet the following
 * requirements in order to avoid undefined behavior:
 *  - The host memory region given by (buffer_origin region) contains the latest bits when the enqueued write command
 * begins execution.
 *  - The buffer object or memory objects created from this buffer object are not mapped.
 *  - The buffer object or memory objects created from this buffer object are not used by any command-queue until the
 * write command has finished execution.
 */
cl_int VC4CL_FUNC(clEnqueueWriteBufferRect)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
    const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch,
    size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueWriteBufferRect, "cl_command_queue", command_queue, "cl_mem", buffer,
        "cl_bool", blocking_write, "const size_t*", buffer_origin, "const size_t*", host_origin, "const size_t*",
        region, "size_t", buffer_row_pitch, "size_t", buffer_slice_pitch, "size_t", host_row_pitch, "size_t",
        host_slice_pitch, "void*", ptr, "cl_uint", num_events_in_wait_list, "const cl_event*", event_wait_list,
        "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(buffer))
    return toType<Buffer>(buffer)->enqueueWriteRect(toType<CommandQueue>(command_queue), blocking_write == CL_TRUE,
        buffer_origin, host_origin, region, buffer_row_pitch, buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr,
        num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 80+:
 *  Enqueues a command to copy a buffer object identified by src_buffer to another buffer object identified by
 * dst_buffer.
 *
 *  \param command_queue refers to the command-queue in which the copy command will be queued.  The OpenCL context
 * associated with command_queue, src_buffer and dst_buffer must be the same.
 *
 *  \param src_offset refers to the offset where to begin copying data from src_buffer.
 *
 *  \param dst_offset refers to the offset where to begin copying data into dst_buffer.
 *
 *  \param size refers to the size in bytes to copy.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed.  If event_wait_list is NULL, then this particular command does not wait on any event to
 * complete.  If event_wait_list is NULL, num_events_in_wait_list must be 0.  If event_wait_list is not NULL, the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and
 * command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function
 * returns.
 *
 *  \param event returns an event object that identifies this particular copy command and can be used to query or queue
 * a wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue await for this command to complete.
 *  clEnqueueBarrierWithWaitList can be used instead. If the event_wait_list and the event arguments are not NULL, the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueCopyBuffer returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one
 * of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue, src_buffer and dst_buffer are not the same or if
 * the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if src_buffer and dst_buffer are not valid buffer objects.
 *  - CL_INVALID_VALUE if src_offset, dst_offset, size, src_offset + size or dst_offset + size require accessing
 * elements outside the src_buffer and dst_buffer buffer objects respectively.
 *  - CL_INVALID_VALUE if size is 0.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if src_buffer is a sub-buffer object and offset specified when the sub-buffer
 * object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if dst_buffer is a sub-buffer object and offset specified when the sub-buffer
 * object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_MEM_COPY_OVERLAP if src_buffer and dst_buffer are the same buffer or sub-buffer object and the source and
 * destination regions overlap or if src_buffer and dst_buffer are different sub-buffers of the same associated buffer
 * object and they overlap. The regions overlap if src_offset <= dst_offset <= src_offset + size – 1 or if dst_offset
 * <= src_offset <= dst_offset + size – 1.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with
 * src_buffer or dst_buffer.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueCopyBuffer)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer,
    size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
    cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueCopyBuffer, "cl_command_queue", command_queue, "cl_mem", src_buffer,
        "cl_mem", dst_buffer, "size_t", src_offset, "size_t", dst_offset, "size_t", size, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(src_buffer))
    CHECK_BUFFER(toType<Buffer>(dst_buffer))
    return toType<Buffer>(src_buffer)
        ->enqueueCopyInto(toType<CommandQueue>(command_queue), toType<Buffer>(dst_buffer), src_offset, dst_offset, size,
            num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 82+:
 *
 *  Enqueues a command to copy a 2D or 3D rectangular region from the buffer object identified by src_buffer to a 2D or
 * 3D region in the buffer object identified by dst_buffer. Copying begins at the source offset and destination offset
 * which are computed as described below in the description for src_origin and dst_origin. Each byte of the region's
 * width is copied from the source offset to the destination offset. After copying each width, the source and
 * destination offsets are incremented by their respective source and destination row pitches. After copying each 2D
 * rectangle, the source and destination offsets are incremented by their respective source and destination slice
 * pitches.
 *
 *  NOTE: If src_buffer and dst_buffer are the same buffer object, src_row_pitch must equal dst_row_pitch and
 * src_slice_pitch must equal dst_slice_pitch.
 *
 *  \param command_queue refers to the command-queue in which the copy command will be queued. The OpenCL context
 * associated with command_queue, src_buffer and dst_buffer must be the same.
 *
 *  \param src_origin defines the (x, y, z) offset in the memory region associated with src_buffer. For a 2D rectangle
 * region, the z value given by src_origin[2] should be 0. The offset in bytes is computed as src_origin[2] *
 * src_slice_pitch + src_origin[1] * src_row_pitch + src_origin[0].
 *
 *  \param dst_origin defines the (x, y, z) offset in the memory region associated with dst_buffer. For a 2D rectangle
 * region, the z value given by dst_origin[2] should be 0. The offset in bytes is computed as dst_origin[2] *
 * dst_slice_pitch + dst_origin[1] * dst_row_pitch + dst_origin[0].
 *
 *  \param region defines the (width in bytes, height in rows, depth in slices) of the 2D or 3D rectangle being copied.
 * For a 2D rectangle, the depth value given by region[2] should be 1. The values in region cannot be 0.
 *
 *  \param src_row_pitch is the length of each row in bytes to be used for the memory region associated with src_buffer.
 * If src_row_pitch is 0, src_row_pitch is computed as region[0].
 *
 *  \param src_slice_pitch is the length of each 2D slice in bytes to be used for the memory region associated with
 * src_buffer. If src_slice_pitch is 0, src_slice_pitch is computed as region[1] * src_row_pitch.
 *
 *  \param dst_row_pitch is the length of each row in bytes to be used for the memory region associated with dst_buffer.
 * If dst_row_pitch is 0, dst_row_pitch is computed as region[0].
 *
 *  \param dst_slice_pitch is the length of each 2D slice in bytes to be used for the memory region associated with
 * dst_buffer. If dst_slice_pitch is 0, dst_slice_pitch is computed as region[1] * dst_row_pitch.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL, the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular copy command and can be used to query or queue
 * a wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete.
 *  clEnqueueBarrierWithWaitList can be used instead. If  the event_wait_list and the event arguments are not NULL , the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueCopyBufferRect returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns
 * one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue, src_buffer and dst_buffer are not the same or if
 * the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if src_buffer and dst_buffer are not valid buffer objects.
 *  - CL_INVALID_VALUE if (src_origin, region, src_row_pitch, src_slice_pitch) or (dst_origin, region, dst_row_pitch,
 * dst_slice_pitch) require accessing elements outside the src_buffer and dst_buffer buffer objects respectively.
 *  - CL_INVALID_VALUE if any region array element is 0.
 *  - CL_INVALID_VALUE if src_row_pitch is not 0 and is less than region[0].
 *  - CL_INVALID_VALUE if dst_row_pitch is not 0 and is less than region[0].
 *  - CL_INVALID_VALUE if src_slice_pitch is not 0 and is less than region[1] * src_row_pitch or if src_slice_pitch is
 * not 0 and is not a multiple of src_row_pitch.
 *  - CL_INVALID_VALUE if dst_slice_pitch is not 0 and is less than region[1] * dst_row_pitch or if dst_slice_pitch is
 * not 0 and is not a multiple of dst_row_pitch.
 *  - CL_INVALID_VALUE if src_buffer and dst_buffer are the same buffer object and src_slice_pitch is not equal to
 * dst_slice_pitch and src_row_pitch is not equal to dst_row_pitch.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MEM_COPY_OVERLAP if src_buffer and dst_buffer are the same buffer or sub-buffer object and the source and
 * destination regions overlap or if src_buffer and dst_buffer are different sub-buffers of the same associated buffer
 * object and they overlap. Refer to Appendix E for details on how to determine if source and destination regions
 * overlap.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if src_buffer is a sub-buffer object and offset specified when the sub-buffer
 * object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if dst_buffer is a sub-buffer object and offset specified when the sub-buffer
 * object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with
 * src_buffer or dst_buffer.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueCopyBufferRect)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer,
    const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch,
    size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueCopyBufferRect, "cl_command_queue", command_queue, "cl_mem", src_buffer,
        "cl_mem", dst_buffer, "const size_t*", src_origin, "const size_t*", dst_origin, "const size_t*", region,
        "size_t", src_row_pitch, "size_t", src_slice_pitch, "size_t", dst_row_pitch, "size_t", dst_slice_pitch,
        "cl_uint", num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(src_buffer))
    CHECK_BUFFER(toType<Buffer>(dst_buffer))

    return toType<Buffer>(src_buffer)
        ->enqueueCopyIntoRect(toType<CommandQueue>(command_queue), toType<Buffer>(dst_buffer), src_origin, dst_origin,
            region, src_row_pitch, src_slice_pitch, dst_row_pitch, dst_slice_pitch, num_events_in_wait_list,
            event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 85+:
 *
 *  Enqueues a command to fill a buffer object with a pattern of a given pattern size. The usage information which
 * indicates whether the memory object can be read or written by a kernel and/or the host and is given by the
 * cl_mem_flags argument value specified when buffer is created is ignored by clEnqueueFillBuffer.
 *
 *  \param command_queue refers to the command-queue in which the fill command will be queued. The OpenCL context
 * associated with command_queue and buffer must be the same.
 *
 *  \param buffer is a valid buffer object.
 *
 *  \param pattern is a pointer to the data pattern of size pattern_size in bytes. pattern will be used to fill a region
 * in buffer starting at offset and is size bytes in size. The data pattern must be a scalar or vector integer or
 * floating-point data type supported by OpenCL as described in sections 6.1.1 and 6.1.2. For example, if buffer is to
 * be filled with a pattern of float4 values, then pattern will be a pointer to a cl_float4 value and pattern_size will
 * be sizeof(cl_float4). The maximum value of pattern_size is the size of the largest integer or floating-point vector
 * data type supported by the OpenCL device. The memory associated with pattern can be reused or freed after the
 * function returns.
 *
 *  \param offset is the location in bytes of the region being filled in buffer and must be a multiple of pattern_size.
 *
 *  \param size is the size in bytes of region being filled in buffer and must be a multiple of pattern_size.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL, then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and
 * command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function
 * returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a
 * wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete.
 *  clEnqueueBarrierWithWaitList can be used instead. If the event_wait_list and the event arguments are not NULL , the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueFillBuffer returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one
 * of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and buffer are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object.
 *  - CL_INVALID_VALUE if offset or offset + size require accessing elements outside the buffer buffer object
 * respectively.
 *  - CL_INVALID_VALUE if pattern is NULL or if pattern_size is 0 or if pattern_size is not one of {1, 2, 4, 8, 16, 32,
 * 64, 128}.
 *  - CL_INVALID_VALUE if offset and size are not a multiple of pattern_size.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object and offset specified when the sub-buffer object
 * is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with buffer.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueFillBuffer)(cl_command_queue command_queue, cl_mem buffer, const void* pattern,
    size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
    cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueFillBuffer, "cl_command_queue", command_queue, "cl_mem", buffer,
        "const void*", pattern, "size_t", pattern_size, "size_t", offset, "size_t", size, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(buffer))

    return toType<Buffer>(buffer)->enqueueFill(toType<CommandQueue>(command_queue), pattern, pattern_size, offset, size,
        num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 87+:
 *
 *  Enqueues a command to map a region of the buffer object given by buffer into the host address space and returns a
 * pointer to this mapped region.
 *
 *  \param command_queue must be a valid command-queue.
 *
 *  \param blocking_map indicates if the map operation is blocking or non-blocking. If blocking_map is CL_TRUE ,
 * clEnqueueMapBuffer does not return until the specified region in buffer is mapped into the host address space and the
 * application can access the contents of the mapped region using the pointer returned by clEnqueueMapBuffer. If
 * blocking_map is CL_FALSE i.e. map operation is non-blocking, the pointer to the mapped region returned by
 * clEnqueueMapBuffer cannot be used until the map command has completed. The event argument returns an event object
 * which can be used to query the execution status of the map command. When the map command is completed, the
 * application can access the contents of the mapped region using the pointer returned by clEnqueueMapBuffer.
 *
 *  \param map_flags is a bit-field and is described in table 5.5.
 *
 *  \param buffer is a valid buffer object. The OpenCL context associated with command_queue and buffer must be the
 * same.
 *
 *  \param offset and size are the offset in bytes and the size of the region in the buffer object that is being mapped.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a
 * wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete. If the event_wait_list
 * and the event arguments are not NULL , the event argument should not refer to an element of the event_wait_list
 * array.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clEnqueueMapBuffer will return a pointer to the mapped region. The errcode_ret is set to CL_SUCCESS.
 *  A NULL pointer is returned otherwise with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and buffer are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if buffer is not a valid buffer object.
 *  - CL_INVALID_VALUE if region being mapped given by (offset, size) is out of bounds or if size is 0 or if values
 * specified in map_flags are not valid.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if buffer is a sub-buffer object and offset specified when the sub-buffer object
 * is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_MAP_FAILURE if there is a failure to map the requested region into the host address space. This error cannot
 * occur for buffer objects created with CL_MEM_USE_HOST_PTR or CL_MEM_ALLOC_HOST_PTR.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the map operation is blocking and the execution status of any of
 * the events in event_wait_list is a negative integer value.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with buffer.
 *  - CL_INVALID_OPERATION if buffer has been created with CL_MEM_HOST_WRITE_ONLY or CL_MEM_HOST_NO_ACCESS and
 * CL_MAP_READ is set in map_flags or if buffer has been created with CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS and
 * CL_MAP_WRITE or CL_MAP_WRITE_INVALIDATE_REGION is set in map_flags.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE: If the buffer object is created with CL_MEM_USE_HOST_PTR set in mem_flags, the following will be true:
 *  - The host_ptr specified in clCreateBuffer is guaranteed to contain the latest bits in the region being mapped when
 * the clEnqueueMapBuffer command has completed.
 *  - The pointer value returned by clEnqueueMapBuffer will be derived from the host_ptr specified when the buffer
 * object is created.
 */
void* VC4CL_FUNC(clEnqueueMapBuffer)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map,
    cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("void*", clEnqueueMapBuffer, "cl_command_queue", command_queue, "cl_mem", buffer, "cl_bool",
        blocking_map, "cl_map_flags", map_flags, "size_t", offset, "size_t", size, "cl_uint", num_events_in_wait_list,
        "const cl_event*", event_wait_list, "cl_event*", event, "cl_int*", errcode_ret);
    CHECK_COMMAND_QUEUE_ERROR_CODE(toType<CommandQueue>(command_queue), errcode_ret, void*)
    CHECK_BUFFER_ERROR_CODE(toType<Buffer>(buffer), errcode_ret, void*)

    return toType<Buffer>(buffer)->enqueueMap(toType<CommandQueue>(command_queue), blocking_map == CL_TRUE, map_flags,
        offset, size, num_events_in_wait_list, event_wait_list, event, errcode_ret);
}

/*!
 * OpenCL 1.2 specification, page 119:
 *
 *  \return Increments the memobj reference count. clRetainMemObject returns CL_SUCCESS if the function is executed
 * successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_MEM_OBJECT if memobj is not a valid memory object (buffer or image object).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  clCreateBuffer, clCreateSubBuffer and clCreateImage perform an implicit retain.
 */
cl_int VC4CL_FUNC(clRetainMemObject)(cl_mem memobj)
{
    VC4CL_PRINT_API_CALL("cl_int", clRetainMemObject, "cl_mem", memobj);
    CHECK_BUFFER(toType<Buffer>(memobj))
    return toType<Buffer>(memobj)->retain();
}

/*!
 * OpenCL 1.2 specification, page 119:
 *
 *  \return Decrements the memobj reference count. clReleaseMemObject returns CL_SUCCESS if the function is executed
 * successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_MEM_OBJECT if memobj is not a valid memory object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  After the memobj reference count becomes zero and commands queued for execution on a command-queue(s) that use
 * memobj have finished, the memory object is deleted. If memobj is a buffer object, memobj cannot be deleted until all
 * sub-buffer objects associated with memobj are deleted.
 */
cl_int VC4CL_FUNC(clReleaseMemObject)(cl_mem memobj)
{
    VC4CL_PRINT_API_CALL("cl_int", clReleaseMemObject, "cl_mem", memobj);
    CHECK_BUFFER(toType<Buffer>(memobj))
    return toType<Buffer>(memobj)->release();
}

/*!
 * OpenCL 1.2 specifcation, pages 120+:
 *
 *  Registers a user callback function with a memory object. Each call to clSetMemObjectDestructorCallback registers the
 * specified user callback function on a callback stack associated with memobj. The registered user callback functions
 * are called in the reverse order in which they were registered. The user callback functions are called and then the
 * memory object’s resources are freed and the memory object is deleted. This provides a mechanism for the application
 * (and libraries) using memobj to be notified when the memory referenced by host_ptr, specified when the memory object
 * is created and used as the storage bits for the memory object, can be reused or freed.
 *
 *  \param memobj is a valid memory object.
 *
 *  \param pfn_notify is the callback function that can be registered by the application. This callback function may be
 * called asynchronously by the OpenCL implementation. It is the application’s responsibility to ensure that the
 * callback function is thread-safe. The parameters to this callback function are: memobj is the memory object being
 * deleted. When the user callback is called by the implementation, this memory object is not longer valid. memobj is
 * only provided for reference purposes. user_data is a pointer to user supplied data. user_data will be passed as
 * the user_data argument when pfn_notify is called. user_data can be NULL.
 *
 *  \return clSetMemObjectDestructorCallback returns CL_SUCCESS if the function is executed successfully. Otherwise, it
 * returns one of the following errors:
 *  - CL_INVALID_MEM_OBJECT if memobj is not a valid memory object.
 *  - CL_INVALID_VALUE if pfn_notify is NULL.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE: When the user callback function is called by the implementation, the contents of the memory region pointed to
 * by host_ptr (if the memory object is created with CL_MEM_USE_HOST_PTR ) are undefined. The callback function is
 * typically used by the application to either free or reuse the memory region pointed to by host_ptr. The behavior of
 * calling expensive system routines, OpenCL API calls to create contexts or command-queues, or blocking OpenCL
 * operations from the following list below, in a callback is undefined. clFinish, clWaitForEvents, blocking calls to
 * clEnqueueReadBuffer, clEnqueueReadBufferRect, clEnqueueWriteBuffer, clEnqueueWriteBufferRect, blocking calls to
 * clEnqueueReadImage and clEnqueueWriteImage, blocking calls to clEnqueueMapBuffer, clEnqueueMapImage, blocking calls
 * to clBuildProgram, clCompileProgram or clLinkProgram If an application needs to wait for completion of a routine
 * from the above list in a callback, please use the non-blocking form of the function, and assign a completion
 * callback to it to do the remainder of your work. Note that when a callback (or other code) enqueues commands to a
 * command-queue, the commands are not required to begin execution until the queue is flushed. In standard usage,
 * blocking enqueue calls serve this role by implicitly flushing the queue. Since blocking calls are not permitted in
 * callbacks, those callbacks that enqueue commands on a command queue should either call clFlush on the queue before
 * returning or arrange for clFlush to be called later on another thread. The user callback function may not call OpenCL
 * APIs with the memory object for which the callback function is invoked and for such cases the behavior of OpenCL
 * APIs is considered to be undefined.
 */
cl_int VC4CL_FUNC(clSetMemObjectDestructorCallback)(
    cl_mem memobj, void(CL_CALLBACK* pfn_notify)(cl_mem memobj, void* user_data), void* user_data)
{
    VC4CL_PRINT_API_CALL("cl_int", clSetMemObjectDestructorCallback, "cl_mem", memobj,
        "void(CL_CALLBACK*)(cl_mem memobj, void* user_data)", &pfn_notify, "void*", user_data);
    CHECK_BUFFER(toType<Buffer>(memobj))
    return toType<Buffer>(memobj)->setDestructorCallback(pfn_notify, user_data);
}

/*!
 * OpenCL 1.2 specification pages 121+:
 *
 *  Enqueues a command to unmap a previously mapped region of a memory object. Reads or writes from the host using the
 * pointer returned by clEnqueueMapBuffer or clEnqueueMapImage are considered to be complete.
 *
 *  \param command_queue must be a valid command-queue.
 *
 *  \param memobj is a valid memory object. The OpenCL context associated with command_queue and memobj must be the
 * same.
 *
 *  \param mapped_ptr is the host address returned by a previous call to clEnqueueMapBuffer, or clEnqueueMapImage for
 * memobj.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before
 * clEnqueueUnmapMemObject can be executed. If event_wait_list is NULL , then clEnqueueUnmapMemObject does not wait on
 * any event to complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not
 * NULL , the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater
 * than 0. The events specified in event_wait_list act as synchronization points. The context associated with events in
 * event_wait_list and command_queue must be the same. The memory associated with event_wait_list can be reused or
 * freed after the function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a
 * wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete.
 *  clEnqueueBarrierWithWaitList can be used instead. If the event_wait_list and the event arguments are not NULL , the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueUnmapMemObject returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns
 * one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_MEM_OBJECT if memobj is not a valid memory object.
 *  - CL_INVALID_VALUE if mapped_ptr is not a valid pointer returned by clEnqueueMapBuffer, or clEnqueueMapImage for
 * memobj.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or if event_wait_list is
 * not NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and memobj are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *
 *  clEnqueueMapBuffer, and clEnqueueMapImage increments the mapped count of the memory object. The initial mapped count
 * value of the memory object is zero. Multiple calls to clEnqueueMapBuffer, or clEnqueueMapImage on the same memory
 * object will increment this mapped count by appropriate number of calls. clEnqueueUnmapMemObject decrements the
 * mapped count of the memory object. clEnqueueMapBuffer, and clEnqueueMapImage act as synchronization points for a
 * region of the buffer object being mapped.
 */
cl_int VC4CL_FUNC(clEnqueueUnmapMemObject)(cl_command_queue command_queue, cl_mem memobj, void* mapped_ptr,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueUnmapMemObject, "cl_command_queue", command_queue, "cl_mem", memobj,
        "void*", mapped_ptr, "cl_uint", num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*",
        event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(memobj))

    return toType<Buffer>(memobj)->enqueueUnmap(
        toType<CommandQueue>(command_queue), mapped_ptr, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 124+:
 *
 *  Enqueues a command to indicate which device a set of memory objects should be associated with. Typically, memory
 * objects are implicitly migrated to a device for which enqueued commands, using the memory object, are targeted.
 * clEnqueueMigrateMemObjects allows this migration to be explicitly performed ahead of the dependent commands. This
 * allows a user to preemptively change the association of a memory object, through regular command queue scheduling,
 * in order to prepare for another upcoming command. This also permits an application to overlap the placement of memory
 * objects with other unrelated operations before these memory objects are needed potentially hiding transfer
 * latencies. Once the event, returned from clEnqueueMigrateMemObjects, has been marked CL_COMPLETE the memory objects
 * specified in mem_objects have been successfully migrated to the device associated with command_queue. The migrated
 * memory object shall remain resident on the device until another command is enqueued that either implicitly or
 * explicitly migrates it away. clEnqueueMigrateMemObjects can also be used to direct the initial placement of a memory
 * object, after creation, possibly avoiding the initial overhead of instantiating the object on the first enqueued
 * command to use it.
 *
 *  The user is responsible for managing the event dependencies, associated with this command, in order to avoid
 * overlapping access to memory objects. Improperly specified event dependencies passed to clEnqueueMigrateMemObjects
 * could result in undefined results.
 *
 *  \param command_queue is a valid command-queue. The specified set of memory objects in mem_objects will be migrated
 * to the OpenCL device associated with command_queue or to the host if the CL_MIGRATE_MEM_OBJECT_HOST has been
 * specified.
 *
 *  \param num_mem_objects is the number of memory objects specified in mem_objects.
 *
 *  \param mem_objects is a pointer to a list of memory objects.
 *
 *  \param flags is a bit-field that is used to specify migration options. The following table describes the possible
 * values for flags.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a
 * wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete. If the event_wait_list
 * and the event arguments are not NULL , the event argument should not refer to an element of the event_wait_list
 * array.
 *
 *  \return clEnqueueMigrateMemObjects return CL_SUCCESS if the function is executed successfully. Otherwise, it returns
 * one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and memory objects in mem_objects are not the same
 * or if the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if any of the memory objects in mem_objects is not a valid memory object.
 *  - CL_INVALID_VALUE if num_mem_objects is zero or if mem_objects is NULL .
 *  - CL_INVALID_VALUE if flags is not 0 or is not any of the values described in the table above.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for the specified set of memory objects
 * in mem_objects.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 */
cl_int VC4CL_FUNC(clEnqueueMigrateMemObjects)(cl_command_queue command_queue, cl_uint num_mem_objects,
    const cl_mem* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueMigrateMemObjects, "cl_command_queue", command_queue, "cl_uint",
        num_mem_objects, "const cl_mem*", mem_objects, "cl_mem_migration_flags", flags, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CommandQueue* commandQueue = toType<CommandQueue>(command_queue);
    if(num_mem_objects == 0 || mem_objects == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "No memory objects set to migrate!");
    for(cl_uint i = 0; i < num_mem_objects; ++i)
    {
        CHECK_BUFFER(toType<Buffer>(mem_objects[i]))
        if(commandQueue->context() != toType<Buffer>(mem_objects[i])->context())
            return returnError(
                CL_INVALID_CONTEXT, __FILE__, __LINE__, "Contexts of command-queue and buffer do not match!");
    }
    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    // All buffers are always on the single device (the VideoCore IV GPU), so no migration is required
    Event* e = newOpenCLObject<Event>(commandQueue->context(), CL_QUEUED, CommandType::BUFFER_MIGRATE);
    CHECK_ALLOCATION(e)
    // this command doesn't actually do anything, since buffers are always located in GPU memory and accessible from
    // host-memory
    EventAction* action = newObject<NoAction>(CL_SUCCESS);
    CHECK_ALLOCATION(action)
    e->action.reset(action);

    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    cl_int status = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(status, event);
}

/*!
 * OpenCL 1.2 specification, pages 126+:
 *
 *  \param memobj specifies the memory object being queried.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information
 * returned in param_value by clGetMemObjectInfo is described in table 5.11.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is
 * NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.11.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data being queried by param_value. If
 * param_value_size_ret is NULL , it is ignored.
 *
 *  \return clGetMemObjectInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return
 * type as described in table 5.11 and param_value is not NULL .
 *  - CL_INVALID_MEM_OBJECT if memobj is a not a valid memory object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetMemObjectInfo)(
    cl_mem memobj, cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetMemObjectInfo, "cl_mem", memobj, "cl_mem_info", param_name, "size_t",
        param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_BUFFER(toType<Buffer>(memobj))
    return toType<Buffer>(memobj)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
