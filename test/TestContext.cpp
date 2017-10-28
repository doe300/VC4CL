/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestContext.h"
#include "src/types.h"
#include "src/icd_loader.h"
#include "src/Context.h"
#include "src/Platform.h"
#include "src/Device.h"

using namespace vc4cl;

TestContext::TestContext() : context(NULL)
{
    TEST_ADD(TestContext::testCreateContext);
    TEST_ADD(TestContext::testCreateContextFromType); 
    TEST_ADD(TestContext::testGetContextInfo);
    TEST_ADD(TestContext::testRetainContext);
    TEST_ADD(TestContext::testReleaseContext);
}

void TestContext::testCreateContext()
{
    cl_int errcode = CL_SUCCESS;
    cl_context_properties props[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties)NULL, 0};
    context = VC4CL_FUNC(clCreateContext)(props, 1, NULL, NULL, NULL, &errcode);
    TEST_ASSERT_EQUALS(nullptr, context);
    TEST_ASSERT(errcode != CL_SUCCESS);
    
    props[1] = reinterpret_cast<cl_context_properties>(Platform::getVC4CLPlatform().toBase());
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(props, 1, &device_id, NULL, NULL, &errcode);
    TEST_ASSERT(context != NULL);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    
    errcode = VC4CL_FUNC(clReleaseContext)(context);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestContext::testCreateContextFromType()
{
    cl_int errcode = CL_SUCCESS;
    cl_context_properties props[3] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(Platform::getVC4CLPlatform().toBase()), 0};
    context = VC4CL_FUNC(clCreateContextFromType)(props, CL_DEVICE_TYPE_ACCELERATOR, NULL, NULL, &errcode);
    TEST_ASSERT_EQUALS(nullptr, context);
    TEST_ASSERT(errcode != CL_SUCCESS);
    
    context = VC4CL_FUNC(clCreateContextFromType)(props, CL_DEVICE_TYPE_DEFAULT, NULL, NULL, &errcode);
    TEST_ASSERT(context != NULL);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestContext::testGetContextInfo()
{
    size_t info_size = 0;
    char buffer[1024];
    cl_int state = VC4CL_FUNC(clGetContextInfo)(context, CL_CONTEXT_REFERENCE_COUNT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetContextInfo)(context, CL_CONTEXT_NUM_DEVICES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetContextInfo)(context, CL_CONTEXT_DEVICES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_id), info_size);
    TEST_ASSERT_EQUALS(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), *reinterpret_cast<cl_device_id*>(buffer));
    
    state = VC4CL_FUNC(clGetContextInfo)(context, CL_CONTEXT_PROPERTIES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(info_size % (2 * sizeof(cl_context_properties)) == 0);
    
    state = VC4CL_FUNC(clGetContextInfo)(context, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

void TestContext::testRetainContext()
{
    TEST_ASSERT_EQUALS(1u, toType<Context>(context)->getReferences());
    cl_int state = VC4CL_FUNC(clRetainContext)(context);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(2u, toType<Context>(context)->getReferences());
    
    //release again, so the next test destroys the context
    state = VC4CL_FUNC(clReleaseContext)(context);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    TEST_ASSERT_EQUALS(1u, toType<Context>(context)->getReferences());
}

void TestContext::testReleaseContext()
{
    TEST_ASSERT_EQUALS(1u, toType<Context>(context)->getReferences());
    cl_int state = VC4CL_FUNC(clReleaseContext)(context);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}
