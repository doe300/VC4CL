/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTPLATFORM_H
#define TESTPLATFORM_H

#include <CL/opencl.h>

#include "cpptest.h"

class TestPlatform : public Test::Suite
{
public:
    TestPlatform();

    void testGetPlatformIDs();
    void testGetPlatformInfo();
    
private:
    cl_platform_id platform;
};

#endif /* TESTPLATFORM_H */

