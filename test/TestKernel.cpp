/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <cstring>

#include "TestKernel.h"
#include "src/Kernel.h"
#include "src/Buffer.h"
#include "src/icd_loader.h"
#include "util.h"

using namespace vc4cl;

static const char kernel_name[] = "hello_world";
static const size_t work_size[vc4cl::kernel_config::NUM_DIMENSIONS] = {1, 2, 1};
static std::string sourceCode;

TestKernel::TestKernel() : context(nullptr), program(nullptr), queue(nullptr), in_buffer(nullptr), out_buffer(nullptr), kernel(nullptr)
{
    TEST_ADD(TestKernel::testCreateKernel);
    TEST_ADD(TestKernel::testCreateKernelsInProgram);
    TEST_ADD(TestKernel::testSetKernelArg);
    TEST_ADD(TestKernel::testGetKernelInfo);
    TEST_ADD(TestKernel::testGetKernelArgInfo);
    TEST_ADD(TestKernel::testGetKernelWorkGroupInfo);
    TEST_ADD(TestKernel::prepareArgBuffer);
    TEST_ADD(TestKernel::testEnqueueNDRangeKernel);
    TEST_ADD(TestKernel::testEnqueueNativeKernel);
    TEST_ADD(TestKernel::testKernelResult);
    TEST_ADD(TestKernel::testEnqueueTask);
    TEST_ADD(TestKernel::testRetainKernel);
    TEST_ADD(TestKernel::testReleaseKernel);
}

static bool checkBuildStatus(const cl_program program)
{
    size_t info_size = 0;
    cl_build_status status = 1;
    cl_int state = VC4CL_FUNC(clGetProgramBuildInfo)(program, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(),
        CL_PROGRAM_BUILD_STATUS, sizeof(status), &status, &info_size);
    if(CL_BUILD_SUCCESS != status)
    {
        // for better error debugging
        std::array<char, 40960> log = {0};
        state = VC4CL_FUNC(clGetProgramBuildInfo)(program, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(),
            CL_PROGRAM_BUILD_LOG, log.size(), log.data(), nullptr);
        std::cerr << "Build log: " << log.data() << std::endl;
    }
    return CL_SUCCESS == state && CL_BUILD_SUCCESS == status;
}

bool TestKernel::setup()
{
    cl_int errcode = CL_SUCCESS;
    sourceCode = readFile("./test/hello_world_vector.cl");
    const std::size_t sourceLength = sourceCode.size();
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
    queue = VC4CL_FUNC(clCreateCommandQueue)(context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0, &errcode);
    const char* strings[1] = {sourceCode.data()};
    program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, strings, &sourceLength, &errcode);
    errcode = VC4CL_FUNC(clBuildProgram)(program, 1, &device_id, nullptr, nullptr, nullptr);
    in_buffer = VC4CL_FUNC(clCreateBuffer)(context, 0, (work_size[0] * work_size[1] * work_size[2]) * sizeof(cl_char16), nullptr, &errcode);
    out_buffer = VC4CL_FUNC(clCreateBuffer)(context, 0, (work_size[0] * work_size[1] * work_size[2]) * sizeof(cl_char16), nullptr, &errcode);
    return errcode == CL_SUCCESS && context && queue && program && checkBuildStatus(program) && in_buffer && out_buffer;
}

void TestKernel::testCreateKernel()
{
    cl_int errcode = CL_SUCCESS;
    kernel = VC4CL_FUNC(clCreateKernel)(program, "no_such_kernel", &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, kernel);
    
    kernel = VC4CL_FUNC(clCreateKernel)(program, kernel_name, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(kernel != nullptr);
}

void TestKernel::testCreateKernelsInProgram()
{
    cl_uint num_kernels = 0;
    cl_kernel kernels[16];
    cl_int state = VC4CL_FUNC(clCreateKernelsInProgram)(program, 16, kernels, &num_kernels);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, num_kernels);
    TEST_ASSERT(kernels[0] != nullptr);
    
    VC4CL_FUNC(clReleaseKernel)(kernels[0]);
}

void TestKernel::testSetKernelArg()
{
    cl_char16 arg0;
    cl_int state = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(arg0), &arg0);
    TEST_ASSERT_EQUALS(CL_INVALID_ARG_SIZE, state);
    
    state = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(in_buffer), &in_buffer);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(out_buffer), &out_buffer);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestKernel::testGetKernelInfo()
{
    size_t info_size = 0;
    char buffer[1024];
    cl_int state = VC4CL_FUNC(clGetKernelInfo)(kernel, CL_KERNEL_FUNCTION_NAME, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(strlen(kernel_name) + 1, info_size);
    TEST_ASSERT(strncmp(kernel_name, buffer, info_size) == 0);
    
    state = VC4CL_FUNC(clGetKernelInfo)(kernel, CL_KERNEL_NUM_ARGS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(2u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelInfo)(kernel, CL_KERNEL_REFERENCE_COUNT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelInfo)(kernel, CL_KERNEL_CONTEXT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_context), info_size);
    TEST_ASSERT_EQUALS(context, *reinterpret_cast<cl_context*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelInfo)(kernel, CL_KERNEL_PROGRAM, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_program), info_size);
    TEST_ASSERT_EQUALS(program, *reinterpret_cast<cl_program*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelInfo)(kernel, CL_KERNEL_ATTRIBUTES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(std::string("reqd_work_group_size(1,1,1)"), buffer);
    
    state = VC4CL_FUNC(clGetKernelInfo)(kernel, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

void TestKernel::testGetKernelWorkGroupInfo()
{
    size_t info_size = 0;
    char buffer[1024];
    cl_int state = VC4CL_FUNC(clGetKernelWorkGroupInfo)(kernel, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_KERNEL_GLOBAL_WORK_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_INVALID_VALUE, state);
    
    state = VC4CL_FUNC(clGetKernelWorkGroupInfo)(kernel, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_KERNEL_WORK_GROUP_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT(*reinterpret_cast<size_t*>(buffer) >= 1u);
    
    state = VC4CL_FUNC(clGetKernelWorkGroupInfo)(kernel, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_KERNEL_COMPILE_WORK_GROUP_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(3 * sizeof(size_t), info_size);
    TEST_ASSERT_EQUALS(1u, reinterpret_cast<size_t*>(buffer)[0]);
    TEST_ASSERT_EQUALS(1u, reinterpret_cast<size_t*>(buffer)[1]);
    TEST_ASSERT_EQUALS(1u, reinterpret_cast<size_t*>(buffer)[2]);
    
    state = VC4CL_FUNC(clGetKernelWorkGroupInfo)(kernel, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_KERNEL_LOCAL_MEM_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_ulong), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_ulong*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelWorkGroupInfo)(kernel, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<size_t*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelWorkGroupInfo)(kernel, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_KERNEL_PRIVATE_MEM_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_ulong), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_ulong*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelWorkGroupInfo)(kernel, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_INVALID_VALUE, state);
}

void TestKernel::testGetKernelArgInfo()
{
    size_t info_size = 0;
    char buffer[1024];
    cl_int state = VC4CL_FUNC(clGetKernelArgInfo)(kernel, 2, CL_KERNEL_ARG_NAME, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_INVALID_ARG_INDEX, state);
    
    state = VC4CL_FUNC(clGetKernelArgInfo)(kernel, 0, CL_KERNEL_ARG_ADDRESS_QUALIFIER, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_kernel_arg_address_qualifier), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL), *reinterpret_cast<cl_kernel_arg_address_qualifier*>(buffer));

    state = VC4CL_FUNC(clGetKernelArgInfo)(kernel, 0, CL_KERNEL_ARG_ACCESS_QUALIFIER, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_kernel_arg_access_qualifier), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_NONE), *reinterpret_cast<cl_kernel_arg_access_qualifier*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelArgInfo)(kernel, 0, CL_KERNEL_ARG_TYPE_NAME, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clGetKernelArgInfo)(kernel, 0, CL_KERNEL_ARG_TYPE_QUALIFIER, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_kernel_arg_type_qualifier), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_CONST), *reinterpret_cast<cl_kernel_arg_type_qualifier*>(buffer));
    
    state = VC4CL_FUNC(clGetKernelArgInfo)(kernel, 0, CL_KERNEL_ARG_NAME, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    //as of CLang 3.9, the parameters are not named anymore
    const std::string arg0Name_clang39("0");
    const std::string arg0Name("in");
    if(info_size == arg0Name.size() + 1)
    {
    	TEST_ASSERT(strncmp(arg0Name.data(), buffer, info_size) == 0);
    }
    else if(info_size == arg0Name_clang39.size() + 1)
    {
    	TEST_ASSERT(strncmp(arg0Name_clang39.data(), buffer, info_size) == 0);
    }
    else
    {
    	TEST_FAIL("Invalid argument name length");
    }
    
    state = VC4CL_FUNC(clGetKernelArgInfo)(kernel, 0, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_INVALID_VALUE, state);
}

static const char input[16] = "Hello World!";

void TestKernel::prepareArgBuffer()
{
    cl_int state = VC4CL_FUNC(clEnqueueFillBuffer)(queue, in_buffer, input, sizeof(input), 0, toType<Buffer>(in_buffer)->deviceBuffer->size, 0, nullptr, nullptr);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    char zero = '\0';
    state = VC4CL_FUNC(clEnqueueFillBuffer)(queue, out_buffer, &zero, 1, 0, toType<Buffer>(out_buffer)->deviceBuffer->size, 0, nullptr, nullptr);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestKernel::testEnqueueNDRangeKernel()
{
    size_t local_size = 5;
    
    cl_event event = nullptr;
    cl_int state = VC4CL_FUNC(clEnqueueNDRangeKernel)(queue, kernel, 1, nullptr, work_size, &local_size, 0, nullptr, &event);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, event);
    
    state = VC4CL_FUNC(clEnqueueNDRangeKernel)(queue, kernel, 3, nullptr, work_size, nullptr, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != nullptr);
    
    state = VC4CL_FUNC(clWaitForEvents)(1, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT(event != nullptr);
    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());
    VC4CL_FUNC(clReleaseEvent)(event);
}

void TestKernel::testEnqueueNativeKernel()
{
    cl_event event = nullptr;
    cl_int state = VC4CL_FUNC(clEnqueueNativeKernel)(queue, nullptr, nullptr, 0, 0, nullptr, nullptr, 0, nullptr, &event);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, event);
}

void TestKernel::testKernelResult()
{
	TEST_ASSERT_EQUALS(strlen(input), strlen(reinterpret_cast<const char*>(toType<Buffer>(in_buffer)->deviceBuffer->hostPointer)));
	TEST_ASSERT_EQUALS(strlen(input), strlen(reinterpret_cast<const char*>(toType<Buffer>(out_buffer)->deviceBuffer->hostPointer)));
	TEST_ASSERT_EQUALS(std::string(input), std::string(reinterpret_cast<const char*>(toType<Buffer>(in_buffer)->deviceBuffer->hostPointer), strlen(input)));
	TEST_ASSERT_EQUALS(std::string(input), std::string(reinterpret_cast<const char*>(toType<Buffer>(out_buffer)->deviceBuffer->hostPointer), strlen(input)));
	DEBUG_LOG(DebugLevel::KERNEL_EXECUTION, {
		char* buffer = reinterpret_cast<char*>(toType<Buffer>(in_buffer)->deviceBuffer->hostPointer);
		printf("[%s:%d] Input buffer: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c (%zu)\n", __FILE__, __LINE__, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15], strlen(buffer));
		buffer = reinterpret_cast<char*>(toType<Buffer>(out_buffer)->deviceBuffer->hostPointer);
		printf("[%s:%d] Output buffer: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c (%zu)\n", __FILE__, __LINE__, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7], buffer[8], buffer[9], buffer[10], buffer[11], buffer[12], buffer[13], buffer[14], buffer[15], strlen(buffer));
	})
}

void TestKernel::testEnqueueTask()
{
    cl_event event = nullptr;
    cl_int state = VC4CL_FUNC(clEnqueueTask)(queue, kernel, 0, nullptr, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(event != nullptr);
    
    state = VC4CL_FUNC(clWaitForEvents)(1, &event);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT_EQUALS(CL_COMPLETE, toType<Event>(event)->getStatus());
    VC4CL_FUNC(clReleaseEvent)(event);
}

void TestKernel::testRetainKernel()
{
    TEST_ASSERT_EQUALS(1u, toType<Kernel>(kernel)->getReferences());
    cl_int state = VC4CL_FUNC(clRetainKernel)(kernel);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    TEST_ASSERT_EQUALS(2u, toType<Kernel>(kernel)->getReferences());
    state = VC4CL_FUNC(clReleaseKernel)(kernel);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, toType<Kernel>(kernel)->getReferences());
}

void TestKernel::testReleaseKernel()
{
    TEST_ASSERT_EQUALS(1u, toType<Kernel>(kernel)->getReferences());
    cl_int state = VC4CL_FUNC(clReleaseKernel)(kernel);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestKernel::tear_down()
{
    VC4CL_FUNC(clReleaseCommandQueue)(queue);
    VC4CL_FUNC(clReleaseProgram)(program);
    VC4CL_FUNC(clReleaseContext)(context);
    VC4CL_FUNC(clReleaseMemObject)(in_buffer);
    VC4CL_FUNC(clReleaseMemObject)(out_buffer);
}
