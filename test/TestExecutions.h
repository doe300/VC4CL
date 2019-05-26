/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TEST_TESTEXECUTIONS_H_
#define TEST_TESTEXECUTIONS_H_

#include "src/vc4cl_config.h"

#include "cpptest.h"

class TestExecutions : public Test::Suite
{
public:
	TestExecutions();

	bool setup() override;

	/*
	 * Is run after every execution and tests whether the VC4 is in a hung state
	 */
	void testHungState();
	void testFibonacci();
	void testFFT2();
	void testHelloWorld();
	void testHelloWorldVector();
	void testBranches();
	void testSFU();
	void testShuffle();
	void testWorkItems();
	void testBarrier();

	void tear_down() override;

private:
	cl_context context;
	cl_command_queue queue;

	void buildProgram(cl_program* program, const std::string& fileName);
};

#endif /* TEST_TESTEXECUTIONS_H_ */
