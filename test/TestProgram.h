/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTPROGRAM_H
#define TESTPROGRAM_H

#include "src/vc4cl_config.h"

#include "cpptest.h"

#include <atomic>

class TestProgram : public Test::Suite
{
public:
    TestProgram();

    bool setup() override;

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

    void tear_down() override;

    unsigned num_callback;
    // for asynchronous compilation
    std::atomic<unsigned> num_pendingCallbacks{};

private:
    cl_context context;
    cl_program source_program;
    cl_program binary_program;

    void checkBuildStatus(const cl_program program);
};

#endif /* TESTPROGRAM_H */
