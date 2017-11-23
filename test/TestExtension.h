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

    bool setup() override;
    
    void runKernel();
    void testPerformanceValues(bool shouldBeZero);
    void testResetPerformanceCounters();
    void testReleasePerformanceCounters();
    void testTrackLiveObjects();
    void trackLiveObject(const std::string& name);
    
    void tear_down() override;
private:
    cl_context context;
    cl_counter_vc4cl counter1;
    cl_counter_vc4cl counter2;
    Test::Output* output;

    uint8_t numLiveContexts;
    uint8_t numLiveCounters;
};

#endif /* TESTEXTENSION_H */

