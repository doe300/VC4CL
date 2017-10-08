/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TESTSYSTEM_H
#define TESTSYSTEM_H

#include "cpptest.h"

class TestSystem : public Test::Suite
{
public:
    TestSystem();
    
    void testGetSystemInfo();

};

#endif /* TESTSYSTEM_H */

