/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTEVENT_H
#define TESTEVENT_H

#include <CL/opencl.h>

#include "cpptest.h"

class TestEvent : public Test::Suite
{
public:
    TestEvent();
    
    virtual bool setup();
    
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
    
    virtual void tear_down();

    size_t event_count;
private:
    cl_context context;
    cl_command_queue queue;
    cl_event user_event;
};

#endif /* TESTEVENT_H */

