/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTCONTEXT_H
#define TESTCONTEXT_H

#include "src/vc4cl_config.h"

#include "cpptest.h"

class TestContext : public Test::Suite
{
public:
    TestContext();
    
    void testCreateContext();
    void testCreateContextFromType();
    void testRetainContext();
    void testReleaseContext();
    void testGetContextInfo();
    
    private:
        cl_context context;
};

#endif /* TESTCONTEXT_H */

