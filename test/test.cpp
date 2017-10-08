/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <cstdlib>
#include <fstream>

#include "TestPlatform.h"
#include "TestDevice.h"
#include "TestContext.h"
#include "TestCommandQueue.h"
#include "TestBuffer.h"
#include "TestProgram.h"
#include "TestKernel.h"
#include "TestEvent.h"
#include "TestImage.h"
#include "TestSystem.h"
#include "TestExtension.h"
#include "TestExecutions.h"

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

    assert(sizeof(cl_int) == 4);
    assert(sizeof(cl_long) == 8);
    assert(sizeof(size_t) == 4);
    assert(offsetof(_cl_context, dispatch) == 0);
    
    //run tests

    TestSystem testSytem;
    testSytem.run(output);

    TestPlatform testPlatform;
    testPlatform.run(output);
    
    TestDevice testDevice;
    testDevice.run(output);
    
    TestContext testContext;
    testContext.run(output);
    
    TestCommandQueue testQueue;
    testQueue.run(output);
    
    TestBuffer testBuffer;
    testBuffer.run(output);
    
    TestImage testImage;
    testImage.run(output);
    
#if HAS_COMPILER
    TestProgram testProgram;
    testProgram.run(output);
    
    TestKernel testKernel;
    testKernel.run(output);
#endif
    
    TestEvent testEvent;
    testEvent.run(output);
    
    TestExtension testExtension(&output);
    testExtension.run(output);

#if HAS_COMPILER
    TestExecutions testExecutions;
    testExecutions.run(output);
#endif

    return 0;
}

