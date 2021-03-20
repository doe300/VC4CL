/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TEST_TESTEXECUTIONS_H_
#define TEST_TESTEXECUTIONS_H_

#include "src/Object.h"
#include "src/Program.h"
#include "src/vc4cl_config.h"

#include "cpptest.h"

#include <string>
#include <unordered_map>
#include <vector>

class TestExecutions : public Test::Suite
{
public:
    TestExecutions();
    explicit TestExecutions(std::vector<std::string>&& customTestNames);

    bool setup() override;

    /*
     * Is run after execution and tests whether the VC4 is in a hung state
     */
    void testHungState();

    void runTestData(std::string dataName);
    void runNoSuchTestData(std::string dataName);

    void tear_down() override;

private:
    cl_context context;
    cl_command_queue queue;

    void buildProgram(cl_program* program, const std::string& fileName);

    std::unordered_map<std::string, vc4cl::object_wrapper<vc4cl::Program>> compilationCache;
};

#endif /* TEST_TESTEXECUTIONS_H_ */
