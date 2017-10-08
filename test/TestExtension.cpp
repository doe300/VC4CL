/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestExtension.h"
#include "TestKernel.h"
#include "src/Platform.h"

using namespace vc4cl;

TestExtension::TestExtension(Test::Output* output) : context(nullptr), write_stalls(nullptr), read_stalls(nullptr), output(output)
{
    TEST_ADD(TestExtension::runKernel);
    TEST_ADD_SINGLE_ARGUMENT(TestExtension::testPerformanceValues, false);
    TEST_ADD(TestExtension::testResetPerformanceCounters);
    TEST_ADD_SINGLE_ARGUMENT(TestExtension::testPerformanceValues, true);
    TEST_ADD(TestExtension::testReleasePerformanceCounters);
}

bool TestExtension::setup()
{
	cl_int state = CL_SUCCESS;
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &state);
	if(state == CL_SUCCESS)
		write_stalls = VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_COUNTER_MEMORY_WRITE_STALL_CYCES_VC4CL, &state);
	if(state == CL_SUCCESS)
		read_stalls = VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_COUNTER_MEMORY_WRITE_STALL_CYCES_VC4CL, &state);
    return state == CL_SUCCESS && context != nullptr && write_stalls != nullptr && read_stalls != nullptr;
}

void TestExtension::tear_down()
{
	VC4CL_FUNC(clReleaseContext)(context);
}

void TestExtension::runKernel()
{
    TestKernel testKernel;
    testKernel.run(*output);
}

void TestExtension::testPerformanceValues(bool shouldBeZero)
{
    cl_uint value = 0;
    cl_int state = VC4CL_FUNC(clGetPerformanceCounterValueVC4CL)(write_stalls, &value);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    if(shouldBeZero)
    {
        TEST_ASSERT_EQUALS(0, value);
    }
    else
    {
        TEST_ASSERT(value > 0);
    }
    
    state = VC4CL_FUNC(clGetPerformanceCounterValueVC4CL)(read_stalls, &value);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    if(shouldBeZero)
    {
        TEST_ASSERT_EQUALS(0, value);
    }
    else
    {
        TEST_ASSERT(value > 0);
    }
}

void TestExtension::testResetPerformanceCounters()
{
    cl_int state = VC4CL_FUNC(clResetPerformanceCounterValueVC4CL)(write_stalls);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clResetPerformanceCounterValueVC4CL)(read_stalls);
	TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestExtension::testReleasePerformanceCounters()
{
    cl_int state = VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(write_stalls);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(read_stalls);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(write_stalls);
    TEST_ASSERT_EQUALS(CL_INVALID_VALUE, state);
}
