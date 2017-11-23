/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestExtension.h"
#include "TestKernel.h"
#include "src/Platform.h"

using namespace vc4cl;

TestExtension::TestExtension(Test::Output* output) : context(nullptr), counter1(nullptr), counter2(nullptr), output(output), numLiveContexts(0), numLiveCounters(0)
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
		counter1 = VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_COUNTER_EXECUTION_CYCLES_VC4CL, &state);
	if(state == CL_SUCCESS)
		counter2 = VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_COUNTER_ARGUMENT_CACHE_HITS_VC4CL, &state);
    return state == CL_SUCCESS && context != nullptr && counter1 != nullptr && counter2 != nullptr;
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
    cl_int state = VC4CL_FUNC(clGetPerformanceCounterValueVC4CL)(counter1, &value);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    if(shouldBeZero)
    {
        TEST_ASSERT_EQUALS(0u, value);
    }
    else
    {
        TEST_ASSERT(value > 0u);
    }
    
    state = VC4CL_FUNC(clGetPerformanceCounterValueVC4CL)(counter2, &value);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    if(shouldBeZero)
    {
        TEST_ASSERT_EQUALS(0u, value);
    }
    else
    {
        TEST_ASSERT(value > 0u);
    }
}

void TestExtension::testResetPerformanceCounters()
{
    cl_int state = VC4CL_FUNC(clResetPerformanceCounterValueVC4CL)(counter1);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clResetPerformanceCounterValueVC4CL)(counter2);
	TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestExtension::testReleasePerformanceCounters()
{
    cl_int state = VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(counter1);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(counter2);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(counter1);
    TEST_ASSERT_EQUALS(CL_INVALID_PERFORMANCE_COUNTER_VC4CL, state);
}

static void liveObjectTrackingWrapper(void* user_data, void* obj_ptr, const char* type_name, cl_uint refcount)
{
	reinterpret_cast<TestExtension*>(user_data)->trackLiveObject(type_name);
}

void TestExtension::testTrackLiveObjects()
{
	TEST_THROWS_NOTHING(VC4CL_FUNC(clTrackLiveObjectsAltera)(Platform::getVC4CLPlatform().toBase()));
	TEST_THROWS_NOTHING(VC4CL_FUNC(clReportLiveObjectsAltera)(Platform::getVC4CLPlatform().toBase(), liveObjectTrackingWrapper, this));

	TEST_ASSERT_EQUALS(static_cast<uint8_t>(2), numLiveCounters);
	TEST_ASSERT_EQUALS(static_cast<uint8_t>(1), numLiveContexts);
}

void TestExtension::trackLiveObject(const std::string& name)
{
	if(name == _cl_context::TYPE_NAME)
		++numLiveContexts;
	if(name == _cl_counter_vc4cl::TYPE_NAME)
		++numLiveCounters;
}
