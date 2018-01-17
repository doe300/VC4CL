/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTIMAGE_H
#define TESTIMAGE_H

#include <CL/opencl.h>

#include "cpptest.h"

class TestImage : public Test::Suite
{
public:
    TestImage();

    /*
     * Checks supported image device-info required by EMBEDDED_PROFILE
     */
    void testImageLimits();
    void testImageFormats();
    void testImageTypes();

    void testHostsideTFormat();
    void testHostsideLTFormat();
    void testHostsideRasterFormat();

    void testDeviceTFormatRead();
    void testDeviceLTFormatRead();
    void testDeviceRasterFormatRead();

    void testDeviceImageWrite();
protected:

    bool setup() override;
    void tear_down() override;

private:
    cl_context context;
    cl_command_queue queue;
};

#endif /* TESTIMAGE_H */

