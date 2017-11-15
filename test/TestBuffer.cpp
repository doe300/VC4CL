/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <string.h>

#include "TestBuffer.h"

#include "src/Buffer.h"
#include "src/icd_loader.h"
#include "src/Device.h"

using namespace vc4cl;

TestBuffer::TestBuffer() : num_callback_called(0), context(nullptr), buffer(nullptr), queue(nullptr), mapped_ptr(nullptr)
{
    TEST_ADD(TestBuffer::testCreateBuffer);
    TEST_ADD(TestBuffer::testCreateSubBuffer);
    TEST_ADD(TestBuffer::testEnqueueReadBuffer);
    TEST_ADD(TestBuffer::testEnqueueWriteBuffer);
    TEST_ADD(TestBuffer::testEnqueueReadBufferRect);
    TEST_ADD(TestBuffer::testEnqueueWriteBufferRect);
    TEST_ADD(TestBuffer::testEnqueueFillBuffer);
    TEST_ADD(TestBuffer::testEnqueueFillBufferRect);
    TEST_ADD(TestBuffer::testEnqueueCopyBuffer);
    TEST_ADD(TestBuffer::testEnqueueCopyBufferRect);
    
    TEST_ADD(TestBuffer::testEnqueueMapBuffer);
    TEST_ADD(TestBuffer::testGetMemObjectInfo);
    TEST_ADD(TestBuffer::testEnqueueUnmapMemObject);
    TEST_ADD(TestBuffer::testEnqueueMigrateMemObjects);
    TEST_ADD(TestBuffer::testRetainMemObject);
    TEST_ADD(TestBuffer::testSetMemObjectDestructorCallback);
    TEST_ADD(TestBuffer::testReleaseMemObject);
}

bool TestBuffer::setup()
{
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
    queue = VC4CL_FUNC(clCreateCommandQueue)(context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0, &errcode);
    return errcode == CL_SUCCESS && context != NULL && queue != NULL;
}


void TestBuffer::testCreateBuffer()
{
    cl_int errcode = CL_SUCCESS;
    buffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_READ_ONLY|CL_MEM_WRITE_ONLY|CL_MEM_USE_HOST_PTR, 1024, nullptr, &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, buffer);
    
    buffer = VC4CL_FUNC(clCreateBuffer)(context, 0, 1024, nullptr, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(buffer != NULL);
}

void TestBuffer::testCreateSubBuffer()
{
    cl_int errcode = CL_SUCCESS;
    cl_mem sub_buffer = VC4CL_FUNC(clCreateSubBuffer)(buffer, CL_MEM_WRITE_ONLY|CL_MEM_READ_ONLY, 0, nullptr, &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, sub_buffer);
    
    TEST_ASSERT_EQUALS(1u, toType<Buffer>(buffer)->getReferences());
    cl_buffer_region region;
    region.origin = 256;
    region.size = 512;
    sub_buffer = VC4CL_FUNC(clCreateSubBuffer)(buffer, 0, CL_BUFFER_CREATE_TYPE_REGION, &region, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(sub_buffer != NULL);
    TEST_ASSERT_EQUALS(2u, toType<Buffer>(buffer)->getReferences());
    errcode = VC4CL_FUNC(clReleaseMemObject)(sub_buffer);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT_EQUALS(1u, toType<Buffer>(buffer)->getReferences());
}

void TestBuffer::testEnqueueReadBuffer()
{
    cl_event event = NULL;
    cl_int state = VC4CL_FUNC(clEnqueueReadBuffer)(queue, buffer, CL_FALSE, 0, 256, nullptr, 0, nullptr, &event);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, event);
    
    char tmp[1024];
    state = VC4CL_FUNC(clEnqueueReadBuffer)(queue, buffer, CL_TRUE, 256, 512, tmp, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != NULL);
    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());
    
    TEST_ASSERT_EQUALS(1u, toType<Event>(event)->getReferences());
    state = VC4CL_FUNC(clReleaseEvent)(event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestBuffer::testEnqueueWriteBuffer()
{
    cl_event event = NULL;
    cl_int state = VC4CL_FUNC(clEnqueueWriteBuffer)(queue, buffer, CL_FALSE, 0, 256, nullptr, 0, nullptr, &event);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, event);
    
    char tmp[1024];
    state = VC4CL_FUNC(clEnqueueWriteBuffer)(queue, buffer, CL_TRUE, 256, 512, tmp, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != NULL);
    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());
    
    TEST_ASSERT_EQUALS(1u, toType<Event>(event)->getReferences());
    state = VC4CL_FUNC(clReleaseEvent)(event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestBuffer::testEnqueueReadBufferRect()
{

}

void TestBuffer::testEnqueueWriteBufferRect()
{

}

void TestBuffer::testEnqueueFillBuffer()
{
    cl_event event = NULL;
    cl_int state = VC4CL_FUNC(clEnqueueFillBuffer)(queue, buffer, nullptr, 17, 0, 256, 0, nullptr, &event);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, event);
    
    char tmp[1024];
    memset(tmp, 0x55, 512);
    state = VC4CL_FUNC(clEnqueueFillBuffer)(queue, buffer, tmp, 8, 256, 512, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != NULL);
    state = VC4CL_FUNC(clWaitForEvents(1, &event));
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());
    
    TEST_ASSERT_EQUALS(1u, toType<Event>(event)->getReferences());
    state = VC4CL_FUNC(clReleaseEvent)(event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestBuffer::testEnqueueFillBufferRect()
{

}

void TestBuffer::testEnqueueCopyBuffer()
{

}

void TestBuffer::testEnqueueCopyBufferRect()
{

}

void TestBuffer::testEnqueueMapBuffer()
{
    cl_int errcode = CL_SUCCESS;
    cl_event event = NULL;
    mapped_ptr = VC4CL_FUNC(clEnqueueMapBuffer)(queue, buffer, CL_TRUE, 0, 256, 1512, 0, nullptr, &event, &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, mapped_ptr);
    TEST_ASSERT_EQUALS(nullptr, event);
    
    mapped_ptr = VC4CL_FUNC(clEnqueueMapBuffer)(queue, buffer, CL_TRUE, 0, 256, 512, 0, nullptr, &event, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(mapped_ptr != NULL);
    TEST_ASSERT(event != NULL);
    
    //value is set by fill-buffer
    TEST_ASSERT_EQUALS(0x55, *static_cast<unsigned char*>(mapped_ptr));
    
    TEST_ASSERT_EQUALS(1u, toType<Event>(event)->getReferences());
    errcode = VC4CL_FUNC(clReleaseEvent)(event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestBuffer::testGetMemObjectInfo()
{
    size_t info_size = 0;
    char tmp[1024];
    cl_int state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_TYPE, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_mem_object_type), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER), *reinterpret_cast<cl_mem_object_type*>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_FLAGS, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_mem_flags), info_size);
    TEST_ASSERT(*reinterpret_cast<cl_mem_flags*>(tmp) & CL_MEM_READ_WRITE);
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_SIZE, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT_EQUALS(static_cast<size_t>(1024), *reinterpret_cast<size_t*>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_HOST_PTR, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(void*), info_size);
    TEST_ASSERT_EQUALS(nullptr, *reinterpret_cast<void**>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_MAP_COUNT, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_REFERENCE_COUNT, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_CONTEXT, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_context), info_size);
    TEST_ASSERT_EQUALS(context, *reinterpret_cast<cl_context*>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_ASSOCIATED_MEMOBJECT, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_mem), info_size);
    TEST_ASSERT_EQUALS(nullptr, *reinterpret_cast<cl_mem*>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, CL_MEM_OFFSET, 1024, tmp, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<size_t*>(tmp));
    
    state = VC4CL_FUNC(clGetMemObjectInfo)(buffer, 0xDEADBEAF, 1024, tmp, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

void TestBuffer::testEnqueueUnmapMemObject()
{
    cl_event event = NULL;
    cl_int state = VC4CL_FUNC(clEnqueueUnmapMemObject)(queue, buffer, mapped_ptr, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != NULL);
    VC4CL_FUNC(clWaitForEvents)(1, &event);
    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());
    state = VC4CL_FUNC(clReleaseEvent)(event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    event = NULL;
    state = VC4CL_FUNC(clEnqueueUnmapMemObject)(queue, buffer, mapped_ptr, 0, nullptr, &event);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, event);
    
    mapped_ptr = NULL;
}

void TestBuffer::testEnqueueMigrateMemObjects()
{

}

void TestBuffer::testRetainMemObject()
{
    TEST_ASSERT_EQUALS(1u, toType<Buffer>(buffer)->getReferences());
    cl_int state = VC4CL_FUNC(clRetainMemObject)(buffer);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    TEST_ASSERT_EQUALS(2u, toType<Buffer>(buffer)->getReferences());
    state = VC4CL_FUNC(clReleaseMemObject)(buffer);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, toType<Buffer>(buffer)->getReferences());
}

static void destructCallback(cl_mem memobj, void* data)
{
    ++reinterpret_cast<TestBuffer*>(data)->num_callback_called;
}

void TestBuffer::testReleaseMemObject()
{
    TEST_ASSERT_EQUALS(1u, toType<Buffer>(buffer)->getReferences());
    cl_int state = VC4CL_FUNC(clReleaseMemObject)(buffer);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    TEST_ASSERT_EQUALS(1u, num_callback_called);
}

void TestBuffer::testSetMemObjectDestructorCallback()
{
    cl_int state = VC4CL_FUNC(clSetMemObjectDestructorCallback)(buffer, &destructCallback, this);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestBuffer::tear_down()
{
    VC4CL_FUNC(clReleaseContext)(context);
    VC4CL_FUNC(clReleaseCommandQueue)(queue);
}
