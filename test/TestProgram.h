/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTPROGRAM_H
#define TESTPROGRAM_H

#include <CL/opencl.h>

#include "cpptest.h"

class TestProgram : public Test::Suite
{
public:
    TestProgram();
    

    virtual bool setup();
    
    void testCreateProgramWithSource();
    void testCreateProgramWithBinary();
    void testCreateProgramWithBuiltinKernels();
    void testRetainProgram();
    void testReleaseProgram();
    void testBuildProgram();
    void testCompileProgram();
    void testLinkProgram();
    void testUnloadPlatformCompiler();
    void testGetProgramInfo();
    void testGetProgramBuildInfo();
    
    virtual void tear_down();
    
    unsigned num_callback;
    
private:
    cl_context context;
    cl_program source_program;
    cl_program binary_program;
};

#endif /* TESTPROGRAM_H */

