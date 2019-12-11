/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <cstring>

#include "TestPlatform.h"
#include "src/extensions.h"
#include "src/icd_loader.h"
#include "src/vc4cl_config.h"

using namespace vc4cl;

TestPlatform::TestPlatform() : platform(nullptr)
{
    TEST_ADD(TestPlatform::testGetPlatformIDs);
    TEST_ADD(TestPlatform::testGetPlatformInfo);
}

void TestPlatform::testGetPlatformIDs()
{
    cl_uint num_ids = 0;
    cl_platform_id ids[8];
    cl_int status_code = VC4CL_FUNC(clGetPlatformIDs)(8, ids, &num_ids);

    TEST_ASSERT_EQUALS(CL_SUCCESS, status_code);
    TEST_ASSERT_EQUALS(1u, num_ids);
    TEST_ASSERT(ids[0] != nullptr);

    platform = ids[0];

    status_code = VC4CL_FUNC(clGetPlatformIDs)(1, nullptr, nullptr);
    TEST_ASSERT(status_code != CL_SUCCESS)

    status_code = VC4CL_FUNC(clGetPlatformIDs)(0, ids, &num_ids);
    TEST_ASSERT(status_code != CL_SUCCESS)
}

void TestPlatform::testGetPlatformInfo()
{
    TEST_ASSERT(platform != nullptr);

    size_t info_size = 0;
    char buffer[1024] = {0};

    cl_int state = VC4CL_FUNC(clGetPlatformInfo)(platform, CL_PLATFORM_NAME, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(platform_config::NAME.compare(buffer) == 0);

    state = VC4CL_FUNC(clGetPlatformInfo)(platform, CL_PLATFORM_PROFILE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(platform_config::PROFILE.compare(buffer) == 0);

    state = VC4CL_FUNC(clGetPlatformInfo)(platform, CL_PLATFORM_VENDOR, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(platform_config::VENDOR.compare(buffer) == 0);

    state = VC4CL_FUNC(clGetPlatformInfo)(platform, CL_PLATFORM_VERSION, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(platform_config::VERSION.compare(buffer) == 0);

    state = VC4CL_FUNC(clGetPlatformInfo)(platform, CL_PLATFORM_ICD_SUFFIX_KHR, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(platform_config::ICD_SUFFIX.compare(buffer) == 0);

    state = VC4CL_FUNC(clGetPlatformInfo)(platform, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_INVALID_VALUE, state);
}
