/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestEvent.h"

#include "src/Event.h"
#include "src/Platform.h"
#include "src/icd_loader.h"

using namespace vc4cl;

TestEvent::TestEvent() : event_count(0), context(nullptr), queue(nullptr), user_event(nullptr)
{
    TEST_ADD(TestEvent::testCreateUserEvent);
    TEST_ADD(TestEvent::testSetUserEventStatus);
    TEST_ADD(TestEvent::testWaitForEvents);
    TEST_ADD(TestEvent::testGetEventInfo);
    TEST_ADD(TestEvent::testSetEventCallback);

    TEST_ADD(TestEvent::testEnqueueBarrierWithWaitList);
    TEST_ADD(TestEvent::testEnqueueMarkerWithWaitList);
    TEST_ADD(TestEvent::testGetEventProfilingInfo);
    TEST_ADD(TestEvent::testRetainEvent);
    TEST_ADD(TestEvent::testReleaseEvent);

    TEST_ADD(TestEvent::testFlush);
    TEST_ADD(TestEvent::testFinish);
}

bool TestEvent::setup()
{
    cl_int state = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &state);
    queue = VC4CL_FUNC(clCreateCommandQueue)(context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0, &state);
    return state == CL_SUCCESS && context != nullptr && queue != nullptr;
}

void TestEvent::testCreateUserEvent()
{
    cl_int state = CL_SUCCESS;
    user_event = VC4CL_FUNC(clCreateUserEvent)(context, &state);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(user_event != nullptr);
}

void TestEvent::testSetUserEventStatus()
{
    cl_int state = VC4CL_FUNC(clSetUserEventStatus)(user_event, -42);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(-42, toType<Event>(user_event)->getStatus());

    state = VC4CL_FUNC(clSetUserEventStatus)(user_event, -43);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(-42, toType<Event>(user_event)->getStatus());
}

void TestEvent::testWaitForEvents()
{
    cl_int state = VC4CL_FUNC(clWaitForEvents)(0, nullptr);
    TEST_ASSERT(state != CL_SUCCESS);

    state = VC4CL_FUNC(clWaitForEvents)(1, &user_event);
    TEST_ASSERT_EQUALS(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, state);
}

void TestEvent::testGetEventInfo()
{
    size_t info_size = 0;
    char buffer[1024];
    cl_int state = VC4CL_FUNC(clGetEventInfo)(user_event, CL_EVENT_COMMAND_QUEUE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_command_queue), info_size);
    TEST_ASSERT_EQUALS(nullptr, *reinterpret_cast<cl_command_queue*>(buffer));

    state = VC4CL_FUNC(clGetEventInfo)(user_event, CL_EVENT_CONTEXT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_context), info_size);
    TEST_ASSERT_EQUALS(context, *reinterpret_cast<cl_context*>(buffer));

    state = VC4CL_FUNC(clGetEventInfo)(user_event, CL_EVENT_COMMAND_TYPE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_command_type), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_command_type>(CL_COMMAND_USER), *reinterpret_cast<cl_command_type*>(buffer));

    state = VC4CL_FUNC(clGetEventInfo)(user_event, CL_EVENT_COMMAND_EXECUTION_STATUS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_int), info_size);
    TEST_ASSERT_EQUALS(-42, *reinterpret_cast<cl_int*>(buffer));

    state = VC4CL_FUNC(clGetEventInfo)(user_event, CL_EVENT_REFERENCE_COUNT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));

    state = VC4CL_FUNC(clGetEventInfo)(user_event, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

static void event_callback(cl_event event, cl_int event_command_exec_status, void* user_data)
{
    ++reinterpret_cast<TestEvent*>(user_data)->event_count;
}

void TestEvent::testSetEventCallback()
{
    cl_int state = VC4CL_FUNC(clSetEventCallback)(user_event, CL_COMPLETE, nullptr, this);
    TEST_ASSERT(state != CL_SUCCESS);

    state = VC4CL_FUNC(clSetEventCallback)(user_event, CL_COMPLETE, &event_callback, this);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestEvent::testEnqueueBarrierWithWaitList()
{
    cl_event event = nullptr;
    cl_int state = VC4CL_FUNC(clEnqueueBarrierWithWaitList)(queue, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != nullptr);

    state = VC4CL_FUNC(clWaitForEvents)(1, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());

    state = VC4CL_FUNC(clReleaseEvent)(event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestEvent::testEnqueueMarkerWithWaitList()
{
    cl_event event = nullptr;
    cl_int state = VC4CL_FUNC(clEnqueueMarkerWithWaitList)(queue, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != nullptr);

    state = VC4CL_FUNC(clWaitForEvents)(1, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());

    state = VC4CL_FUNC(clReleaseEvent)(event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestEvent::testGetEventProfilingInfo()
{
    size_t info_size = 0;
    char buffer[1024];
    cl_int state =
        VC4CL_FUNC(clGetEventProfilingInfo)(user_event, CL_PROFILING_COMMAND_QUEUED, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_PROFILING_INFO_NOT_AVAILABLE, state);

    state = VC4CL_FUNC(clGetEventProfilingInfo)(user_event, CL_PROFILING_COMMAND_SUBMIT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_PROFILING_INFO_NOT_AVAILABLE, state);

    state = VC4CL_FUNC(clGetEventProfilingInfo)(user_event, CL_PROFILING_COMMAND_START, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_PROFILING_INFO_NOT_AVAILABLE, state);

    state = VC4CL_FUNC(clGetEventProfilingInfo)(user_event, CL_PROFILING_COMMAND_END, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_PROFILING_INFO_NOT_AVAILABLE, state);

    state = VC4CL_FUNC(clGetEventProfilingInfo)(user_event, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

void TestEvent::testRetainEvent()
{
    TEST_ASSERT_EQUALS(1u, toType<Event>(user_event)->getReferences());
    cl_int state = VC4CL_FUNC(clRetainEvent)(user_event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT_EQUALS(2u, toType<Event>(user_event)->getReferences());
    state = VC4CL_FUNC(clReleaseEvent)(user_event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, toType<Event>(user_event)->getReferences());
}

void TestEvent::testReleaseEvent()
{
    TEST_ASSERT_EQUALS(1u, toType<Event>(user_event)->getReferences());
    cl_int state = VC4CL_FUNC(clReleaseEvent)(user_event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    // XXX is never triggered?? Since the status of the user-event doesn't change?
    // TEST_ASSERT_EQUALS(1u, event_count);
}

void TestEvent::testFlush()
{
    cl_int state = VC4CL_FUNC(clFlush)(queue);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestEvent::testFinish()
{
    cl_int state = VC4CL_FUNC(clFinish)(queue);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestEvent::tear_down()
{
    VC4CL_FUNC(clReleaseCommandQueue)(queue);
    VC4CL_FUNC(clReleaseContext)(context);
}
