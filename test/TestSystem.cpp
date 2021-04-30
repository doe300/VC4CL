/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestSystem.h"
#include "src/hal/V3D.h"
#include "src/hal/hal.h"

#include <CL/cl_platform.h>

using namespace vc4cl;

TestSystem::TestSystem()
{
    if(!system()->getV3DIfAvailable())
        return;
    TEST_ADD(TestSystem::testGetSystemInfo);
}

void TestSystem::testGetSystemInfo()
{
    auto v3d = system()->getV3DIfAvailable();
    uint32_t res = v3d->getSystemInfo(SystemInfo::VPM_MEMORY_SIZE);
    TEST_ASSERT(res > 1024);
    res = v3d->getSystemInfo(SystemInfo::SEMAPHORES_COUNT);
    TEST_ASSERT_EQUALS(16u, res);
    res = v3d->getSystemInfo(SystemInfo::SLICE_TMU_COUNT);
    TEST_ASSERT_EQUALS(2u, res);
    res = v3d->getSystemInfo(SystemInfo::SLICE_QPU_COUNT);
    TEST_ASSERT_EQUALS(4u, res);
    res = v3d->getSystemInfo(SystemInfo::SLICES_COUNT);
    TEST_ASSERT_EQUALS(3u, res);
    res = v3d->getSystemInfo(SystemInfo::QPU_COUNT);
    TEST_ASSERT_EQUALS(12u, res);
}
