/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTBUFFER_H
#define TESTBUFFER_H

#include "src/vc4cl_config.h"

#include "cpptest.h"

class TestBuffer : public Test::Suite
{
public:
    TestBuffer();
    
    bool setup() override;
    
    void testCreateBuffer();
    void testCreateSubBuffer();
    void testEnqueueReadBuffer();
    void testEnqueueWriteBuffer();
    void testEnqueueReadBufferRect();
    void testEnqueueWriteBufferRect();
    void testEnqueueFillBuffer();
    void testEnqueueFillBufferRect();
    void testEnqueueCopyBuffer();
    void testEnqueueCopyBufferRect();
    
    void testEnqueueMapBuffer();
    void testEnqueueUnmapMemObject();
    void testEnqueueMigrateMemObjects();
    void testGetMemObjectInfo();
    void testRetainMemObject();
    void testReleaseMemObject();
    void testSetMemObjectDestructorCallback();
    
    void tear_down() override;
    
    size_t num_callback_called;
    
private:
    cl_context context;
    cl_mem buffer;
    cl_command_queue queue;
    void* mapped_ptr;

};

#endif /* TESTBUFFER_H */

