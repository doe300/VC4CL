/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTBUILTINS_H
#define TESTBUILTINS_H

#include "cpptest.h"
#include "CL/opencl.h"

class TestBuiltins : Test::Suite
{
public:
    TestBuiltins();
    
    bool setup() override;
    bool before(const std::string& methodName) override;

    void testMathFunctions();
    void testIntegerFunctions();
    void testCommonFunctions();
    void testGeometricFunctions();
    void testRelationalFunctions();
    void testVectorFunctions();
    void testAsyncFunctions();
    void testAtomicFunctions();
    void testImageFunctions();

    void after(const std::string& methodName, const bool success) override;
    void tear_down() override;
    
private:
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    
    cl_kernel kernel;
    cl_mem in;
    cl_mem out0;
    cl_mem out1;
};

#endif /* TESTBUILTINS_H */

