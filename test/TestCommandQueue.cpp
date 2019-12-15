/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestCommandQueue.h"
#include "src/CommandQueue.h"
#include "src/Context.h"
#include "src/Device.h"
#include "src/Platform.h"
#include "src/extensions.h"
#include "src/icd_loader.h"

using namespace vc4cl;

TestCommandQueue::TestCommandQueue() : context(nullptr), queue(nullptr)
{
    TEST_ADD(TestCommandQueue::testCreateCommandQueue);
    TEST_ADD(TestCommandQueue::testCreateCommandQueueWithProperties);
    TEST_ADD(TestCommandQueue::testSetCommandQueueProperties);
    TEST_ADD(TestCommandQueue::testGetCommandQueueInfo);
    TEST_ADD(TestCommandQueue::testRetainCommandQueue);
    TEST_ADD(TestCommandQueue::testReleaseCommandQueue);
}

void TestCommandQueue::testCreateCommandQueue()
{
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(context != nullptr);

    queue = VC4CL_FUNC(clCreateCommandQueue)(context, nullptr, 0, &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, queue);

    queue = VC4CL_FUNC(clCreateCommandQueue)(
        context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_QUEUE_PROFILING_ENABLE, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(queue != nullptr);
}

void TestCommandQueue::testCreateCommandQueueWithProperties()
{
    cl_int errcode = CL_SUCCESS;
    cl_queue_properties_khr props[3] = {CL_QUEUE_PROPERTIES + 15, CL_QUEUE_PROFILING_ENABLE, 0};

    auto dummyQueue = VC4CL_FUNC(clCreateCommandQueueWithPropertiesKHR)(
        context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), props, &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, dummyQueue);

    props[0] = {CL_QUEUE_PROPERTIES};
    dummyQueue = VC4CL_FUNC(clCreateCommandQueueWithPropertiesKHR)(
        context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), props, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(dummyQueue != nullptr);

    TEST_ASSERT_EQUALS(1u, toType<CommandQueue>(queue)->getReferences());
    cl_int state = VC4CL_FUNC(clReleaseCommandQueue)(dummyQueue);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestCommandQueue::testSetCommandQueueProperties()
{
    cl_command_queue_properties oldProps{};
    cl_int state =
        VC4CL_FUNC(clSetCommandQueueProperty)(queue, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_TRUE, &oldProps);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT_EQUALS(CL_QUEUE_PROFILING_ENABLE, oldProps);

    // reset original properties
    state = VC4CL_FUNC(clSetCommandQueueProperty)(queue, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, CL_FALSE, nullptr);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestCommandQueue::testGetCommandQueueInfo()
{
    size_t info_size = 0;
    char buffer[1024] = {0};
    cl_int state = VC4CL_FUNC(clGetCommandQueueInfo)(queue, CL_QUEUE_CONTEXT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_context), info_size);
    TEST_ASSERT_EQUALS(context, *reinterpret_cast<cl_context*>(buffer));

    state = VC4CL_FUNC(clGetCommandQueueInfo)(queue, CL_QUEUE_DEVICE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_id), info_size);
    TEST_ASSERT_EQUALS(toType<Context>(context)->device->toBase(), *reinterpret_cast<cl_device_id*>(buffer));

    state = VC4CL_FUNC(clGetCommandQueueInfo)(queue, CL_QUEUE_REFERENCE_COUNT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));

    state = VC4CL_FUNC(clGetCommandQueueInfo)(queue, CL_QUEUE_PROPERTIES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_command_queue_properties), info_size);
    TEST_ASSERT(*reinterpret_cast<cl_command_queue_properties*>(buffer) & CL_QUEUE_PROFILING_ENABLE);

    state = VC4CL_FUNC(clGetCommandQueueInfo)(queue, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

void TestCommandQueue::testRetainCommandQueue()
{
    TEST_ASSERT_EQUALS(1u, toType<CommandQueue>(queue)->getReferences());
    cl_int state = VC4CL_FUNC(clRetainCommandQueue)(queue);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(2u, toType<CommandQueue>(queue)->getReferences());

    // release again, so the next test destroys the queue
    state = VC4CL_FUNC(clReleaseCommandQueue)(queue);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT_EQUALS(1u, toType<CommandQueue>(queue)->getReferences());
}

void TestCommandQueue::testReleaseCommandQueue()
{
    TEST_ASSERT_EQUALS(1u, toType<CommandQueue>(queue)->getReferences());
    cl_int state = VC4CL_FUNC(clReleaseCommandQueue)(queue);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT_EQUALS(1u, toType<Context>(context)->getReferences());
    state = VC4CL_FUNC(clReleaseContext)(context);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}
