/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTDEVICE_H
#define TESTDEVICE_H

#include <CL/opencl.h>
#include "cpptest.h"

class TestDevice : public Test::Suite
{
public:
    TestDevice();

    void testGetDeviceIDs();
    void testGetDeviceInfo();
    void testCreateSubDevice();
    void testRetainDevice();
    void testReleaseDevice();
    
private:
    cl_device_id device;
};

#endif /* TESTDEVICE_H */

