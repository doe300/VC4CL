/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestExecutions.h"
#include "src/Buffer.h"
#include "src/icd_loader.h"
#include "src/V3D.h"
#include "util.h"

using namespace vc4cl;

static std::string sourceFibonacci;
static std::string sourceFFT;

static constexpr int COUNTER_IDLE = 0;
static constexpr int COUNTER_EXECUTIONS = 1;

// TODO add execution tests for:
// - multiple work-groups
// - with/without "loop-work-groups" optimization enabled

TestExecutions::TestExecutions() : Test::Suite(), context(nullptr), queue(nullptr)
{
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testHelloWorld);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testHelloWorldVector);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testBranches);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testSFU);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testShuffle);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testWorkItems);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testBarrier);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testConstantHelloWorld);
	TEST_ADD(TestExecutions::testHungState);
	TEST_ADD(TestExecutions::testConstantGlobalLoad);
	TEST_ADD(TestExecutions::testHungState);
	//FIXME TEST_ADD(TestExecutions::testFibonacci);
	//TEST_ADD(TestExecutions::testFFT2);
}

bool TestExecutions::setup()
{
	cl_int errcode = CL_SUCCESS;
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
	queue = VC4CL_FUNC(clCreateCommandQueue)(context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0, &errcode);

	return errcode == CL_SUCCESS && context != nullptr && queue != nullptr
			&& V3D::instance()->setCounter(COUNTER_IDLE, CounterType::IDLE_CYCLES) && V3D::instance()->setCounter(COUNTER_EXECUTIONS, CounterType::EXECUTION_CYCLES);
}

void TestExecutions::testHungState()
{
	//If an execution test-case has finished (successful or timed-out) and the VC4 is still active,
	//it is in a hung state (or another program is using it!)

	//reset previous counter values
	V3D::instance()->resetCounterValue(COUNTER_IDLE);
	V3D::instance()->resetCounterValue(COUNTER_EXECUTIONS);

	//wait some amount
	std::this_thread::sleep_for(std::chrono::seconds{1});

	//read new counter values
	auto qpuIdle = static_cast<float>(V3D::instance()->getCounter(COUNTER_IDLE));
	auto qpuExec = static_cast<float>(V3D::instance()->getCounter(COUNTER_EXECUTIONS));

	if(qpuIdle >= 0 && qpuExec >= 0 && (qpuIdle + qpuExec) > 0)
	{
		//if either is -1, the VC4 is powered down, so it cannot be hung

		//one QPU is 1/12 of full power -> 8%
		// -> to be safe use a threshold of 0.04
		float qpuUsage = qpuExec / (qpuIdle + qpuExec);
		TEST_ASSERT_MSG(qpuUsage < 0.04f, "QPU(s) in a hung state or another program is using them!");
	}
}

void TestExecutions::testHelloWorld()
{
	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/hello_world.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, sizeof(cl_char16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "hello_world", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	std::array<char, sizeof(cl_char16)> result;
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, outBuffer, CL_TRUE, 0, result.size(), result.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_STRING_EQUALS("Hello World!", result.data());

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testHelloWorldVector()
{
	const std::string message = "Hello World!";

	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/hello_world_vector.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem inBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_WRITE_ONLY|CL_MEM_READ_ONLY, sizeof(cl_char16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(inBuffer != nullptr);
	errcode = VC4CL_FUNC(clEnqueueWriteBuffer)(queue, inBuffer, CL_TRUE, 0, message.size(), message.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, sizeof(cl_char16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "hello_world", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &inBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	std::array<char, sizeof(cl_char16)> result;
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, outBuffer, CL_TRUE, 0, result.size(), result.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_STRING_EQUALS(message, result.data());

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(inBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testBranches()
{
	//TODO
}

void TestExecutions::testSFU()
{
	const std::array<float, 16> input = {1.0f, 2.0f, 8.0f, 32.0f, 128.0f, 25.70f, 11.1f, 10.240f, 1.5f, 2.7f, 9.0f, 124.340f, 112.2334455f, 56.7f, 74.1f, 0.00001f};

	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/test_sfu.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem inBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_WRITE_ONLY|CL_MEM_READ_ONLY, sizeof(cl_float16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(inBuffer != nullptr);
	errcode = VC4CL_FUNC(clEnqueueWriteBuffer)(queue, inBuffer, CL_TRUE, 0, input.size() * sizeof(float), input.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, 4 * sizeof(cl_float16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "test_sfu", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	std::array<float, 4 * input.size()> result;
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, outBuffer, CL_TRUE, 0, result.size() * sizeof(float), result.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	for(unsigned i = 0; i < 16; ++i)
	{
		//TODO commented-out lines fail for every entry, always with result +inf (recip, rsqrt and -inf log2)
		//TEST_ASSERT_DELTA(1.0f / input.at(i), result.at(i), 1.0f / input.at(i));
		//TEST_ASSERT_DELTA(1.0f / std::sqrt(input.at(i)), result.at(16 + i), 1.0f / std::sqrt(input.at(i)));
		TEST_ASSERT_DELTA(std::exp2(input.at(i)), result.at(32 + i), std::exp2(input.at(i)));
		//TEST_ASSERT_DELTA(std::log2(input.at(i)), result.at(48 + i), std::log2(input.at(i)));
	}

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(inBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testShuffle()
{
	const std::array<char, 2 * sizeof(cl_char16)> input = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/test_shuffle.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem inBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_WRITE_ONLY|CL_MEM_READ_ONLY, 2 * sizeof(cl_char16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(inBuffer != nullptr);
	errcode = VC4CL_FUNC(clEnqueueWriteBuffer)(queue, inBuffer, CL_TRUE, 0, input.size(), input.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, 8 * sizeof(cl_char16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "test_shuffle", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &inBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	const std::array<char, 7 * sizeof(cl_char16)> expected = {
			0x7, 0x6, 0x4, 0x8, 0x1, 0xc, 0xd, 0x1, 0x0, 0x9, 0xe, 0xf, 0x4, 0x3, 0x8, 0x6,
			0x1, 0x7, 0xb, 0x12, 0x15, 0xf, 0x8, 0x9, 0x0, 0x13, 0x2, 0x1, 0x11, 0xd, 0x7, 0x8,
			0x1a, 0x1b, 0x2, 0x10, 0x4, 0x19, 0x6, 0x17, 0x8, 0x9, 0x1c, 0x13, 0x1a, 0xd, 0xe, 0xf,
			0x11, 0x1, 0x2, 0x10, 0x11, 0x1, 0x2, 0x10, 0x11, 0x1, 0x2, 0x10, 0x11, 0x1, 0x2, 0x10,
			0x0, 0x0, 0x0, 0x1, 0x11, 0x0, 0x0, 0x10, 0x0, 0x11, 0x1, 0x2, 0x2, 0x10, 0x0, 0x0,
			//decimal on purpose:
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
			16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	};
	std::array<char, 7 * sizeof(cl_char16)> result;
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, outBuffer, CL_TRUE, 0, result.size(), result.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	for(unsigned i = 0; i < expected.size(); ++i)
		TEST_ASSERT_EQUALS(static_cast<unsigned>(expected.at(i)), static_cast<unsigned>(result.at(i)));

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(inBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testWorkItems()
{
	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/test_work_item.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, 24 * sizeof(cl_uint), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "test_work_item", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	std::array<cl_uint, 24> result;
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, outBuffer, CL_TRUE, 0, result.size() * sizeof(cl_uint), result.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	TEST_ASSERT_EQUALS(1u, result.at(0));
	TEST_ASSERT_EQUALS(1u, result.at(1));
	TEST_ASSERT_EQUALS(1u, result.at(2));
	TEST_ASSERT_EQUALS(1u, result.at(3));
	TEST_ASSERT_EQUALS(0u, result.at(4));
	TEST_ASSERT_EQUALS(0u, result.at(5));
	TEST_ASSERT_EQUALS(0u, result.at(6));
	TEST_ASSERT_EQUALS(0u, result.at(7));
	TEST_ASSERT_EQUALS(0u, result.at(8));
	TEST_ASSERT_EQUALS(0u, result.at(9));
	TEST_ASSERT_EQUALS(1u, result.at(10));
	TEST_ASSERT_EQUALS(1u, result.at(11));
	TEST_ASSERT_EQUALS(1u, result.at(12));
	TEST_ASSERT_EQUALS(0u, result.at(13));
	TEST_ASSERT_EQUALS(0u, result.at(14));
	TEST_ASSERT_EQUALS(0u, result.at(15));
	TEST_ASSERT_EQUALS(1u, result.at(16));
	TEST_ASSERT_EQUALS(1u, result.at(17));
	TEST_ASSERT_EQUALS(1u, result.at(18));
	TEST_ASSERT_EQUALS(0u, result.at(19));
	TEST_ASSERT_EQUALS(0u, result.at(20));
	TEST_ASSERT_EQUALS(0u, result.at(21));

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testBarrier()
{
	const size_t numQPUs = 2;
	const size_t numRounds = 2;
	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/test_barrier.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, 12 * numRounds * numQPUs * sizeof(cl_uint), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "test_barrier", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	std::array<size_t, 3> globalSizes = {numQPUs, numRounds, 0};
	errcode = VC4CL_FUNC(clEnqueueNDRangeKernel)(queue, kernel, 2, nullptr, globalSizes.data(), nullptr, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	std::array<cl_uint, 12 * numRounds * numQPUs> result;
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, outBuffer, CL_TRUE, 0, result.size() * sizeof(cl_uint), result.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	TEST_ASSERT_EQUALS(0u, result.at(0));
	TEST_ASSERT_EQUALS(1u, result.at(1));
	TEST_ASSERT_EQUALS(2u, result.at(2));
	//offset 3 is not set in kernel
	TEST_ASSERT_EQUALS(4u, result.at(4));
	TEST_ASSERT_EQUALS(5u, result.at(5));
	TEST_ASSERT_EQUALS(6u, result.at(6));
	TEST_ASSERT_EQUALS(7u, result.at(7));
	TEST_ASSERT_EQUALS(8u, result.at(8));
	TEST_ASSERT_EQUALS(9u, result.at(9));
	TEST_ASSERT_EQUALS(10u, result.at(10));

	TEST_ASSERT_EQUALS(0u, result.at(12));
	TEST_ASSERT_EQUALS(1u, result.at(13));
	TEST_ASSERT_EQUALS(2u, result.at(14));
	//offset 3 is not set in kernel
	TEST_ASSERT_EQUALS(4u, result.at(16));
	TEST_ASSERT_EQUALS(5u, result.at(17));
	TEST_ASSERT_EQUALS(6u, result.at(18));
	TEST_ASSERT_EQUALS(7u, result.at(19));
	TEST_ASSERT_EQUALS(8u, result.at(20));
	TEST_ASSERT_EQUALS(9u, result.at(21));
	TEST_ASSERT_EQUALS(10u, result.at(22));

	TEST_ASSERT_EQUALS(0u, result.at(24));
	TEST_ASSERT_EQUALS(1u, result.at(25));
	TEST_ASSERT_EQUALS(2u, result.at(26));
	//offset 3 is not set in kernel
	TEST_ASSERT_EQUALS(4u, result.at(28));
	TEST_ASSERT_EQUALS(5u, result.at(29));
	TEST_ASSERT_EQUALS(6u, result.at(30));
	TEST_ASSERT_EQUALS(7u, result.at(31));
	TEST_ASSERT_EQUALS(8u, result.at(32));
	TEST_ASSERT_EQUALS(9u, result.at(33));
	TEST_ASSERT_EQUALS(10u, result.at(34));

	TEST_ASSERT_EQUALS(0u, result.at(36));
	TEST_ASSERT_EQUALS(1u, result.at(37));
	TEST_ASSERT_EQUALS(2u, result.at(38));
	//offset 3 is not set in kernel
	TEST_ASSERT_EQUALS(4u, result.at(40));
	TEST_ASSERT_EQUALS(5u, result.at(41));
	TEST_ASSERT_EQUALS(6u, result.at(42));
	TEST_ASSERT_EQUALS(7u, result.at(43));
	TEST_ASSERT_EQUALS(8u, result.at(44));
	TEST_ASSERT_EQUALS(9u, result.at(45));
	TEST_ASSERT_EQUALS(10u, result.at(46));

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testFibonacci()
{
	const int startVal = 1;

	//setup
	cl_int errcode = CL_SUCCESS;
	sourceFibonacci = readFile("./test/fibonacci.cl");
	const std::size_t sourceLength = sourceFibonacci.size();
	const char* sorceText = sourceFibonacci.data();
	cl_program program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, &sorceText, &sourceLength, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(program != nullptr);
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	errcode = VC4CL_FUNC(clBuildProgram)(program, 1, &device_id, "", nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, sizeof(int) * 16, nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);
	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "fibonacci", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(int), &startVal);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(int), &startVal);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	//FIXME fails for different Context, somehow Program completely fucks-up the Context passed to the creation-functions (The parameter context of VC4CL_FUNC(clCreateBuffer) becomes 0x0 somewhere in the method-body?!)
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 2, sizeof(outBuffer), outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	int* result = static_cast<int*>(toType<Buffer>(outBuffer)->deviceBuffer->hostPointer);
	TEST_ASSERT_EQUALS(2, result[0]);
	TEST_ASSERT_EQUALS(3, result[1]);
	TEST_ASSERT_EQUALS(5, result[2]);
	TEST_ASSERT_EQUALS(8, result[3]);
	TEST_ASSERT_EQUALS(13, result[4]);
	TEST_ASSERT_EQUALS(21, result[5]);
	TEST_ASSERT_EQUALS(34, result[6]);
	TEST_ASSERT_EQUALS(55, result[7]);
	TEST_ASSERT_EQUALS(89, result[8]);
	TEST_ASSERT_EQUALS(144, result[9]);

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testConstantHelloWorld()
{
	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/hello_world_constant.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, sizeof(cl_char16), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "hello_world", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	size_t workSize = 12;
	errcode = VC4CL_FUNC(clEnqueueNDRangeKernel)(queue, kernel, 1, nullptr, &workSize, nullptr, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	std::array<char, sizeof(cl_char16)> result;
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, outBuffer, CL_TRUE, 0, result.size(), result.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_STRING_EQUALS("Hello World!", result.data());

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testConstantGlobalLoad()
{
	//setup
	cl_int errcode = CL_SUCCESS;
	cl_program program = nullptr;
	buildProgram(&program, "./test/constant_load.cl");
	TEST_ASSERT(program != nullptr);

	cl_mem out0Buffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, 2 * sizeof(uint32_t), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(out0Buffer != nullptr);
	
	cl_mem out1Buffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, 2 * sizeof(uint32_t), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(out1Buffer != nullptr);
	
	cl_mem out2Buffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, 2 * sizeof(uint32_t), nullptr, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(out2Buffer != nullptr);

	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "test_constant_load", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != nullptr);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(cl_mem), &out0Buffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(cl_mem), &out1Buffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 2, sizeof(cl_mem), &out2Buffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	uint32_t index = 0;
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 3, sizeof(uint32_t), &index);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = nullptr;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != nullptr);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	std::array<uint32_t, 2> result0 = {0};
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, out0Buffer, CL_TRUE, 0, result0.size() * sizeof(uint32_t), result0.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT_EQUALS(0x42, result0[0]);
	TEST_ASSERT_EQUALS(0x17 + 42, result0[1]);

	std::array<uint16_t, 2> result1 = {0};
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, out1Buffer, CL_TRUE, 0, result1.size() * sizeof(uint16_t), result1.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT_EQUALS(0x42, result1[0]);
	TEST_ASSERT_EQUALS(0x17 + 42, result1[1]);

	std::array<uint8_t, 2> result2 = {0};
	errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue, out2Buffer, CL_TRUE, 0, result2.size() * sizeof(uint8_t), result2.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT_EQUALS(0x42, static_cast<uint32_t>(result2[0]));
	TEST_ASSERT_EQUALS(0x17 + 42, static_cast<uint32_t>(result2[1]));

	//tear-down
	errcode = VC4CL_FUNC(clReleaseEvent)(event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseKernel)(kernel);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(out2Buffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(out1Buffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseMemObject)(out0Buffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clReleaseProgram)(program);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestExecutions::testFFT2()
{
	//setup
	cl_int errcode = CL_SUCCESS;
	sourceFFT = readFile("./test/fft2_2.cl");
	const std::size_t sourceLength = sourceFFT.size();
	const char* sorceText = sourceFFT.data();
	cl_program program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, &sorceText, &sourceLength, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(program != nullptr);
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	errcode = VC4CL_FUNC(clBuildProgram)(program, 1, &device_id, "", nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	//TODO

	//execution
	//test
	//tear-down
	TEST_ASSERT_EQUALS(CL_SUCCESS, VC4CL_FUNC(clReleaseProgram)(program));
}

void TestExecutions::tear_down()
{
	VC4CL_FUNC(clReleaseCommandQueue)(queue);
	VC4CL_FUNC(clReleaseContext)(context);
	V3D::instance()->disableCounter(COUNTER_IDLE);
	V3D::instance()->disableCounter(COUNTER_EXECUTIONS);
}

void TestExecutions::buildProgram(cl_program* program, const std::string& fileName)
{
	cl_int errcode = CL_SUCCESS;
	sourceFibonacci = readFile(fileName);
	const std::size_t sourceLength = sourceFibonacci.size();
	const char* sourceText = sourceFibonacci.data();
	*program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, &sourceText, &sourceLength, &errcode);
	if(errcode != CL_SUCCESS)
		*program = nullptr;
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(*program != nullptr);
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	errcode = VC4CL_FUNC(clBuildProgram)(*program, 1, &device_id, "", nullptr, nullptr);
	if(errcode != CL_SUCCESS)
		*program = nullptr;
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}
