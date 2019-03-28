/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <cstdlib>
#include <fstream>

#include "cpptest-main.h"

#include "TestBuffer.h"
#include "TestCommandQueue.h"
#include "TestContext.h"
#include "TestDevice.h"
#include "TestEvent.h"
#include "TestExecutions.h"
#include "TestExtension.h"
#include "TestImage.h"
#include "TestKernel.h"
#include "TestPlatform.h"
#include "TestProgram.h"
#include "TestSystem.h"

#include "src/Context.h"

using namespace std;

/*
 *
 */
int main(int argc, char** argv)
{
#if TEST_OUTPUT_CONSOLE == 1
    Test::TextOutput output(Test::TextOutput::Verbose);
#else
    std::ofstream file;
    file.open("testResult.log", std::ios_base::out | std::ios_base::trunc);

    Test::TextOutput output(Test::TextOutput::Verbose, file);
#endif

#if use_cl_khr_icd
    assert(offsetof(_cl_context, dispatch) == 0);
#endif

    // run tests

    Test::registerSuite(Test::newInstance<TestSystem>, "system", "Test retrieval of system information");
    Test::registerSuite(Test::newInstance<TestPlatform>, "platform", "Test querying of platform information");
    Test::registerSuite(Test::newInstance<TestDevice>, "device", "Test querying of device information");
    Test::registerSuite(Test::newInstance<TestContext>, "context", "Test creating and querying contexts");
    Test::registerSuite(Test::newInstance<TestCommandQueue>, "queue", "Test creating and querying command queues");
    Test::registerSuite(Test::newInstance<TestBuffer>, "buffer", "Test creating, querying and accessing buffers");
    Test::registerSuite(Test::newInstance<TestImage>, "images", "Test creating, querying and accessing images");

#if HAS_COMPILER
    Test::registerSuite(Test::newInstance<TestProgram>, "programs", "Test building and querying programs");
    Test::registerSuite(Test::newInstance<TestKernel>, "kernels", "Test creating, querying and executing kernels");
#endif

    Test::registerSuite(Test::newInstance<TestEvent>, "events", "Test creating, querying and scheduling events");

#if HAS_COMPILER
    Test::registerSuite([&output]() -> Test::Suite* { return new TestExtension(&output); }, "extensions",
        "Tests supported OpenCL extensions");
    Test::registerSuite(
        Test::newInstance<TestExecutions>, "executions", "Tests the executions and results of a few selected kernels");
#endif

    return Test::runSuites(argc, argv);
}
