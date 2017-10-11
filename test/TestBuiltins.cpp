/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestBuiltins.h"
#include "src/Buffer.h"
#include "src/Device.h"

#include <cmath>

using namespace vc4cl;

#define GET(buffer, type, index) ((type*)toType<Buffer>(buffer)->deviceBuffer->hostPointer)[index]

TestBuiltins::TestBuiltins() : context(nullptr), queue(nullptr), program(nullptr), in(nullptr), out0(nullptr), out1(nullptr)
{
    TEST_ADD(TestBuiltins::testMathFunctions);
    TEST_ADD(TestBuiltins::testIntegerFunctions);
    TEST_ADD(TestBuiltins::testCommonFunctions);
    TEST_ADD(TestBuiltins::testGeometricFunctions);
    TEST_ADD(TestBuiltins::testRelationalFunctions);
    TEST_ADD(TestBuiltins::testVectorFunctions);
    TEST_ADD(TestBuiltins::testAsyncFunctions);
    TEST_ADD(TestBuiltins::testAtomicFunctions);
    TEST_ADD(TestBuiltins::testImageFunctions);
}

bool TestBuiltins::setup()
{
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(NULL, 1, &device_id, NULL, NULL, &errcode);
    queue = VC4CL_FUNC(clCreateCommandQueue)(context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0, &errcode);
    
    //TODO create and compile program
    
    return errcode == CL_SUCCESS && context != NULL && queue != NULL && program != NULL;
}

bool TestBuiltins::before(const std::string& methodName)
{
    cl_int state = -1;
    size_t allocateInput = 0;
    size_t allocateOutput0 = 0;
    size_t allocateOutput1 = 0;
    if(methodName.compare("testMathFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_math", &state);
        allocateOutput0 = sizeof(cl_float16) * 50;
        allocateOutput1 = sizeof(cl_int16) * 50;
    }
    else if(methodName.compare("testIntegerFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_integer", &state);
        allocateOutput0 = sizeof(cl_int16) * 20;
    }
    else if(methodName.compare("testCommonFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_common", &state);
        allocateOutput0 = sizeof(cl_int16) * 10;
    }
    else if(methodName.compare("testGeometricFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_geometric", &state);
        allocateOutput0 = sizeof(cl_int16) * 10;
    }
    else if(methodName.compare("testRelationalFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_relational", &state);
        allocateOutput0 = sizeof(cl_int16) * 20;
    }
    else if(methodName.compare("testVectorFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_vector", &state);
        allocateInput = sizeof(cl_float16) * 4;
        allocateOutput0 = sizeof(cl_float16) * 4;
    }
    else if(methodName.compare("testAsyncFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_async", &state);
        allocateInput = sizeof(cl_float16) * 20;
        allocateOutput0 = sizeof(cl_float16) * 20;
    }
    else if(methodName.compare("testAtomicFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_atomic", &state);
        allocateInput = sizeof(cl_int) * 16;
        allocateOutput0 = sizeof(cl_int) * 16;
    }
    else if(methodName.compare("testImageFunctions") == 0)
    {
        kernel = VC4CL_FUNC(clCreateKernel)(program, "test_image", &state);
        allocateOutput0 = sizeof(cl_float4) * 40;
        allocateOutput1 = sizeof(cl_float4) * 40;
    }
    if(state == CL_SUCCESS && allocateInput > 0)
    {
        in = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_READ_WRITE, allocateInput, nullptr, &state);
    }
    if(state == CL_SUCCESS && allocateOutput0 > 0)
    {
        out0 = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_READ_WRITE, allocateOutput0, nullptr, &state);
    }
    if(state == CL_SUCCESS && allocateOutput1 > 0)
    {
        out1 = VC4CL_FUNC(clCreateBuffer)(context, CL_MEM_READ_WRITE, allocateOutput1, nullptr, &state);
    }
    //TODO need more buffer, for all input-vectors too
    
    return state == CL_SUCCESS;
}

void TestBuiltins::testMathFunctions()
{
    const float val = 0.5f;
    const float val2 = -0.75f;
    const int val3 = 13;
    
    cl_int state = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(float), &val);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(float), &val2);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clSetKernelArg)(kernel, 2, sizeof(int), &val3);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    cl_event event = NULL;
    state = VC4CL_FUNC(clEnqueueTask(queue, kernel, 0, NULL, &event));
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    int i = 0;
    //check result
    TEST_ASSERT_EQUALS_MSG(std::acos(val), GET(out0, float, i++), "Result of acos is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::acosh(val), GET(out0, float, i++), "Result of acosh is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::acos(val * static_cast<float>(M_PI)), GET(out0, float, i++), "Result of acospi is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::asin(val), GET(out0, float, i++), "Result of asin is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::asinh(val), GET(out0, float, i++), "Result of asinh is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::asin(val * static_cast<float>(M_PI)), GET(out0, float, i++), "Result of asinpi is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::atan(val), GET(out0, float, i++), "Result of atan is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::atan2(val, val2), GET(out0, float, i++), "Result of atan2 is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::atanh(val), GET(out0, float, i++), "Result of atanh is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::atan(val * static_cast<float>(M_PI)), GET(out0, float, i++), "Result of atanpi is wrong!");
    i++;//XXX atan2pi
    //cbrt
    TEST_ASSERT_EQUALS_MSG(std::ceil(val), GET(out0, float, i++), "Result of ceil is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::copysign(val, val2), GET(out0, float, i++), "Result of copysign is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::cos(val), GET(out0, float, i++), "Result of cos is wrong!");
    //cosh
    TEST_ASSERT_EQUALS_MSG(std::cos(val * static_cast<float>(M_PI)), GET(out0, float, i++), "Result of cospi is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::erfc(val), GET(out0, float, i++), "Result of erfc is wrong!");
    //erf
    TEST_ASSERT_EQUALS_MSG(std::exp(val), GET(out0, float, i++), "Result of exp is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::exp2(val), GET(out0, float, i++), "Result of exp2 is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::pow(10.0f, val), GET(out0, float, i++), "Result of exp10 is wrong!");
    //expm1
    TEST_ASSERT_EQUALS_MSG(std::fabs(val), GET(out0, float, i++), "Result of fabs is wrong!");
    //fdim
    TEST_ASSERT_EQUALS_MSG(std::floor(val), GET(out0, float, i++), "Result of floor is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::fma(val, val2, val2), GET(out0, float, i++), "Result of fma is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::fmax(val, val2), GET(out0, float, i++), "Result of fmax is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::fmin(val, val2), GET(out0, float, i++), "Result of fmin is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::fmod(val, val2), GET(out0, float, i++), "Result of fmod is wrong!");
    i++;//XXX fract
    //frexp
    TEST_ASSERT_EQUALS_MSG(std::hypot(val, val2), GET(out0, float, i++), "Result of hypot is wrong!");
    //ilogb
    //lgamma
    //lgamma_r
    TEST_ASSERT_EQUALS_MSG(std::log(val), GET(out0, float, i++), "Result of log is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::log2(val), GET(out0, float, i++), "Result of log2 is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::log10(val), GET(out0, float, i++), "Result of log10 is wrong!");
    i++;//XXX log1p
    //logb
    i++;//XXX mad
    //maxmag
    //minmag
    i++; i++;//XXX modf
    //nan
    TEST_ASSERT_EQUALS_MSG(std::nextafter(val, val2), GET(out0, float, i++), "Result of nextafter is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::pow(val, val2), GET(out0, float, i++), "Result of pow is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::pow(val, val3), GET(out0, float, i++), "Result of pown is wrong!");
    //powr
    //remainder
    //remqou
    TEST_ASSERT_EQUALS_MSG(std::rint(val), GET(out0, float, i++), "Result of rint is wrong!");
    //rootn
    TEST_ASSERT_EQUALS_MSG(std::round(val), GET(out0, float, i++), "Result of round is wrong!");
    TEST_ASSERT_EQUALS_MSG(1.0f/std::sqrt(val), GET(out0, float, i++), "Result of rsqrt is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::sin(val), GET(out0, float, i++), "Result of sin is wrong!");
    i++; i++; //XXX sincos
    //sinh
	TEST_ASSERT_EQUALS_MSG(std::sin(val * static_cast<float>(M_PI)), GET(out0, float, i++), "Result of sinpi is wrong!");
	TEST_ASSERT_EQUALS_MSG(std::sqrt(val), GET(out0, float, i++), "Result of sqrt is wrong!");
	TEST_ASSERT_EQUALS_MSG(std::tan(val), GET(out0, float, i++), "Result of tan is wrong!");
	TEST_ASSERT_EQUALS_MSG(std::tanh(val), GET(out0, float, i++), "Result of tanh is wrong!");
	//tanpi
	//tgamma
	TEST_ASSERT_EQUALS_MSG(std::trunc(val), GET(out0, float, i++), "Result of trunc is wrong!");
}

void TestBuiltins::testIntegerFunctions()
{
	const int val = -13;
	const int val2 = 42;
	
	cl_int state = VC4CL_FUNC(clSetKernelArg)(kernel, 0, sizeof(int), &val);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clSetKernelArg)(kernel, 1, sizeof(int), &val2);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clSetKernelArg)(kernel, 2, sizeof(struct _cl_mem), out0);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    cl_event event = NULL;
    state = VC4CL_FUNC(clEnqueueTask(queue, kernel, 0, NULL, &event));
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    int i = 0;
    //check result
    TEST_ASSERT_EQUALS_MSG(std::abs(val), GET(out0, int, i++), "Result of abs is wrong!");
    i++;//XXX abs_diff
    i++;//XXX add_sat
    i++;//XXX hadd
    i++;//XXX rhadd
    i++;//XXX clamp
    i++;//XXX clz
    i++;//XXX mad_hi
    i++;//XXX mad_sat
    TEST_ASSERT_EQUALS_MSG(std::max(val, val2), GET(out0, int, i++), "Result of max is wrong!");
    TEST_ASSERT_EQUALS_MSG(std::min(val, val2), GET(out0, int, i++), "Result of min is wrong!");
    i++;//XXX mul_hi
    i++;//XXX rotate
    i++;//XXX sub_sat
    i++;//XXX upsample
    i++;//XXX mad24
    TEST_ASSERT_EQUALS_MSG(val * val2, GET(out0, int, i++), "Result of mul24 is wrong!");
}

void TestBuiltins::testCommonFunctions()
{

}

void TestBuiltins::testGeometricFunctions()
{

}

void TestBuiltins::testRelationalFunctions()
{

}

void TestBuiltins::testVectorFunctions()
{

}

void TestBuiltins::testAsyncFunctions()
{

}

void TestBuiltins::testAtomicFunctions()
{

}

void TestBuiltins::testImageFunctions()
{

}

void TestBuiltins::after(const std::string& methodName, const bool success)
{
    cl_int state = VC4CL_FUNC(clReleaseKernel)(kernel);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    if(in != nullptr)
    {
        state = VC4CL_FUNC(clReleaseMemObject)(in);
        TEST_ASSERT_EQUALS(CL_SUCCESS, state);
        in = nullptr;
    }
    if(out0 != nullptr)
    {
        state = VC4CL_FUNC(clReleaseMemObject)(out0);
        TEST_ASSERT_EQUALS(CL_SUCCESS, state);
        out0 = nullptr;
    }
    if(out1 != nullptr)
    {
        state = VC4CL_FUNC(clReleaseMemObject)(out1);
        TEST_ASSERT_EQUALS(CL_SUCCESS, state);
        out1 = nullptr;
    }
}

void TestBuiltins::tear_down()
{
    cl_int state = VC4CL_FUNC(clReleaseProgram)(program);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clReleaseCommandQueue)(queue);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    state = VC4CL_FUNC(clReleaseContext)(context);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}
