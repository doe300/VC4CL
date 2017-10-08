/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TEST_TESTEXECUTIONS_H_
#define TEST_TESTEXECUTIONS_H_

#include <CL/opencl.h>

#include "cpptest.h"

class TestExecutions : public Test::Suite
{
public:
	TestExecutions();

	virtual bool setup();

	void testFibonacci();
	void testFFT2();
	//TODO more tests, md4/5, sha1, ...

	virtual void tear_down();

private:
	cl_context context;
	cl_command_queue queue;
};

#endif /* TEST_TESTEXECUTIONS_H_ */
