/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestExecutions.h"
#include "src/Buffer.h"
#include "src/icd_loader.h"
#include "util.h"

using namespace vc4cl;

static std::string sourceFibonacci;
static std::string sourceFFT;

TestExecutions::TestExecutions() : Test::Suite(), context(NULL), queue(NULL)
{
	TEST_ADD(TestExecutions::testFibonacci);
	TEST_ADD(TestExecutions::testFFT2);
}

bool TestExecutions::setup()
{
	cl_int errcode = CL_SUCCESS;
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	context = VC4CL_FUNC(clCreateContext)(NULL, 1, &device_id, NULL, NULL, &errcode);
	queue = VC4CL_FUNC(clCreateCommandQueue)(context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0, &errcode);

	return errcode == CL_SUCCESS && context != NULL && queue != NULL;
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
	TEST_ASSERT(program != NULL);
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	errcode = VC4CL_FUNC(clBuildProgram)(program, 1, &device_id, "", NULL, NULL);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_HOST_READ_ONLY|CL_MEM_WRITE_ONLY, sizeof(int) * 16, NULL, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(outBuffer != NULL);
	cl_kernel kernel = VC4CL_FUNC(clCreateKernel)(program, "fibonacci", &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(kernel != NULL);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(int), &startVal);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(int), &startVal);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	errcode = VC4CL_FUNC(clSetKernelArg)(kernel, 2, sizeof(outBuffer), outBuffer);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//execution
	cl_event event = NULL;
	errcode = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, NULL, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(event != NULL);
	errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

	//test
	int* result = (int*)toType<Buffer>(outBuffer)->deviceBuffer->hostPointer;
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

void TestExecutions::testFFT2()
{
	//setup
	cl_int errcode = CL_SUCCESS;
	sourceFFT = readFile("./test/fft2_2.cl");
	const std::size_t sourceLength = sourceFFT.size();
	const char* sorceText = sourceFFT.data();
	cl_program program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, &sorceText, &sourceLength, &errcode);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	TEST_ASSERT(program != NULL);
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	errcode = VC4CL_FUNC(clBuildProgram)(program, 1, &device_id, "", NULL, NULL);
	TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
	//TODO

	//execution
	//test
	//tear-down
}

void TestExecutions::tear_down()
{
	VC4CL_FUNC(clReleaseCommandQueue)(queue);
	VC4CL_FUNC(clReleaseContext)(context);
}
