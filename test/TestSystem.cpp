/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestSystem.h"
#include "src/V3D.h"

#include <CL/cl_platform.h>

using namespace vc4cl;

TestSystem::TestSystem()
{
    //"warm-up" V3D hardware
    V3D::instance();
    TEST_ADD(TestSystem::testGetSystemInfo);
}

void TestSystem::testGetSystemInfo()
{
    uint32_t res = V3D::instance()->getSystemInfo(SystemInfo::VPM_MEMORY_SIZE);
    TEST_ASSERT(res > 1024);
    res = V3D::instance()->getSystemInfo(SystemInfo::SEMAPHORES_COUNT);
    TEST_ASSERT_EQUALS(16u, res);
    res = V3D::instance()->getSystemInfo(SystemInfo::SLICE_TMU_COUNT);
    TEST_ASSERT_EQUALS(2u, res);
    res = V3D::instance()->getSystemInfo(SystemInfo::SLICE_QPU_COUNT);
    TEST_ASSERT_EQUALS(4u, res);
    res = V3D::instance()->getSystemInfo(SystemInfo::SLICES_COUNT);
    TEST_ASSERT_EQUALS(3u, res);
    res = V3D::instance()->getSystemInfo(SystemInfo::QPU_COUNT);
    TEST_ASSERT_EQUALS(12u, res);
}
