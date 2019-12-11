/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestContext.h"
#include "src/Context.h"
#include "src/Device.h"
#include "src/Platform.h"
#include "src/extensions.h"
#include "src/icd_loader.h"
#include "src/types.h"

using namespace vc4cl;

TestContext::TestContext() : context(nullptr)
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
    cl_context_properties props[7] = {0xDEAD, reinterpret_cast<cl_context_properties>(nullptr),
        CL_CONTEXT_INTEROP_USER_SYNC, CL_TRUE, CL_CONTEXT_MEMORY_INITIALIZE_KHR, CL_CONTEXT_MEMORY_INITIALIZE_LOCAL_KHR,
        0};
    // invalid property key
    context = VC4CL_FUNC(clCreateContext)(props, 1, nullptr, nullptr, nullptr, &errcode);
    TEST_ASSERT_EQUALS(nullptr, context);
    TEST_ASSERT(errcode != CL_SUCCESS);

    // invalid property value
    props[0] = CL_CONTEXT_PLATFORM;
    context = VC4CL_FUNC(clCreateContext)(props, 1, nullptr, nullptr, nullptr, &errcode);
    TEST_ASSERT_EQUALS(nullptr, context);
    TEST_ASSERT(errcode != CL_SUCCESS);

    // valid properties
    props[1] = reinterpret_cast<cl_context_properties>(Platform::getVC4CLPlatform().toBase());
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(props, 1, &device_id, nullptr, nullptr, &errcode);
    TEST_ASSERT(context != nullptr);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

void TestContext::testCreateContextFromType()
{
    cl_int errcode = CL_SUCCESS;
    cl_context_properties props[3] = {
        CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(Platform::getVC4CLPlatform().toBase()), 0};
    auto tmpContext =
        VC4CL_FUNC(clCreateContextFromType)(props, CL_DEVICE_TYPE_ACCELERATOR, nullptr, nullptr, &errcode);
    TEST_ASSERT_EQUALS(nullptr, tmpContext);
    TEST_ASSERT(errcode != CL_SUCCESS);

    tmpContext = VC4CL_FUNC(clCreateContextFromType)(props, CL_DEVICE_TYPE_DEFAULT, nullptr, nullptr, &errcode);
    TEST_ASSERT(tmpContext != nullptr);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);

    errcode = VC4CL_FUNC(clReleaseContext)(tmpContext);
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
    // 2 entries per property + 1 terminating 0-entry (4 bytes each)
    TEST_ASSERT_EQUALS(sizeof(cl_context_properties), info_size % (2 * sizeof(cl_context_properties)));
    TEST_ASSERT_EQUALS(7 * sizeof(cl_context_properties), info_size);

    const cl_context_properties expectedProps[7] = {CL_CONTEXT_PLATFORM,
        reinterpret_cast<cl_context_properties>(Platform::getVC4CLPlatform().toBase()), CL_CONTEXT_INTEROP_USER_SYNC,
        CL_TRUE, CL_CONTEXT_MEMORY_INITIALIZE_KHR, CL_CONTEXT_MEMORY_INITIALIZE_LOCAL_KHR, 0};
    for(unsigned i = 0; i < 7; ++i)
    {
        TEST_ASSERT_EQUALS(expectedProps[i], reinterpret_cast<cl_context_properties*>(buffer)[i]);
    }

    state = VC4CL_FUNC(clGetContextInfo)(context, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

void TestContext::testRetainContext()
{
    TEST_ASSERT_EQUALS(1u, toType<Context>(context)->getReferences());
    cl_int state = VC4CL_FUNC(clRetainContext)(context);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(2u, toType<Context>(context)->getReferences());

    // release again, so the next test destroys the context
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
