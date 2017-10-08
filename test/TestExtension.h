/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTEXTENSION_H
#define TESTEXTENSION_H

#include "cpptest.h"
#include "src/extensions.h"

class TestExtension : public Test::Suite
{
public:
    TestExtension(Test::Output* output);

    virtual bool setup();
    virtual void tear_down();
    
    void runKernel();
    void testPerformanceValues(bool shouldBeZero);
    void testResetPerformanceCounters();
    void testReleasePerformanceCounters();
    
private:
    cl_context context;
    cl_counter_vc4cl write_stalls;
    cl_counter_vc4cl read_stalls;
    Test::Output* output;
};

#endif /* TESTEXTENSION_H */

