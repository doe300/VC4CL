/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTEVENT_H
#define TESTEVENT_H

#include "src/vc4cl_config.h"

#include "cpptest.h"

class TestEvent : public Test::Suite
{
public:
    TestEvent();
    
    bool setup() override;
    
    void testCreateUserEvent();
    void testSetUserEventStatus();
    void testWaitForEvents();
    void testGetEventInfo();
    void testSetEventCallback();
    void testRetainEvent();
    void testReleaseEvent();
    
    void testEnqueueMarkerWithWaitList();
    void testEnqueueBarrierWithWaitList();
    void testGetEventProfilingInfo();
    
    void testFlush();
    void testFinish();
    
    void tear_down() override;

    size_t event_count;
private:
    cl_context context;
    cl_command_queue queue;
    cl_event user_event;
};

#endif /* TESTEVENT_H */

