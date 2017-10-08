/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <CL/cl_platform.h>

#include "TestSystem.h"
#include "src/V3D.h"

using namespace vc4cl;

TestSystem::TestSystem()
{
    TEST_ADD(TestSystem::testGetSystemInfo);
}

void TestSystem::testGetSystemInfo()
{
    cl_uint res = V3D::instance().getSystemInfo(SystemInfo::ARM_MEMORY_SIZE);
    TEST_ASSERT(res > 1024 * 1024);
    res = V3D::instance().getSystemInfo(SystemInfo::VC_MEMORY_SIZE);
    TEST_ASSERT(res > 1024 * 1024);
    res = V3D::instance().getSystemInfo(SystemInfo::VC_MEMORY_BASE);
    TEST_ASSERT(res > 1024 * 1024);
    res = V3D::instance().getSystemInfo(SystemInfo::ARM_MEMORY_BASE);
    //clocks from 600MHz to 700MHz
    TEST_ASSERT_DELTA((cl_uint)600000000, res, (cl_uint)100000000);
    res = V3D::instance().getSystemInfo(SystemInfo::CORE_CLOCK_RATE);
    TEST_ASSERT_EQUALS(250000000, res);
    res = V3D::instance().getSystemInfo(SystemInfo::V3D_CLOCK_RATE);
    TEST_ASSERT_EQUALS(250000000, res);
    res = V3D::instance().getSystemInfo(SystemInfo::VPM_MEMORY_SIZE);
    TEST_ASSERT(res > 1024);
    res = V3D::instance().getSystemInfo(SystemInfo::SEMAPHORES_COUNT);
    TEST_ASSERT_EQUALS(16, res);
    res = V3D::instance().getSystemInfo(SystemInfo::SLICE_TMU_COUNT);
    TEST_ASSERT_EQUALS(2, res);
    res = V3D::instance().getSystemInfo(SystemInfo::SLICE_QPU_COUNT);
    TEST_ASSERT_EQUALS(4, res);
    res = V3D::instance().getSystemInfo(SystemInfo::SLICES_COUNT);
    TEST_ASSERT_EQUALS(3, res);
    res = V3D::instance().getSystemInfo(SystemInfo::QPU_COUNT);
    TEST_ASSERT_EQUALS(12, res);
}
