/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestExecutions.h"
#include "src/Buffer.h"
#include "src/CommandQueue.h"
#include "src/Kernel.h"
#include "src/Program.h"
#include "src/V3D.h"
#include "src/icd_loader.h"
#include "util.h"

#include "TestData.h"

using namespace vc4cl;

static std::string sourceFibonacci;
static std::string sourceFFT;

static constexpr int COUNTER_IDLE = 0;
static constexpr int COUNTER_EXECUTIONS = 1;

// TODO add execution tests for:
// - multiple work-groups
// - with/without "loop-work-groups" optimization enabled

TestExecutions::TestExecutions() : Test::Suite(), context(nullptr), queue(nullptr)
{
    for(const auto& test : test_data::getAllTests())
    {
        TEST_ADD_WITH_STRING(TestExecutions::runTestData, test.first);
    }
    TEST_ADD(TestExecutions::testHungState);
}

TestExecutions::TestExecutions(std::vector<std::string>&& customTestNames) :
    Test::Suite(), context(nullptr), queue(nullptr)
{
    for(const auto& testName : customTestNames)
    {
        if(test_data::getTest(testName))
        {
            TEST_ADD_WITH_STRING(TestExecutions::runTestData, testName);
        }
        else
        {
            TEST_ADD_WITH_STRING(TestExecutions::runNoSuchTestData, testName);
        }
    }
    TEST_ADD(TestExecutions::testHungState);
}

bool TestExecutions::setup()
{
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
    queue =
        VC4CL_FUNC(clCreateCommandQueue)(context, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), 0, &errcode);

    return errcode == CL_SUCCESS && context != nullptr && queue != nullptr &&
        V3D::instance()->setCounter(COUNTER_IDLE, CounterType::IDLE_CYCLES) &&
        V3D::instance()->setCounter(COUNTER_EXECUTIONS, CounterType::EXECUTION_CYCLES);
}

void TestExecutions::testHungState()
{
    // If an execution test-case has finished (successful or timed-out) and the VC4 is still active,
    // it is in a hung state (or another program is using it!)

    // reset previous counter values
    V3D::instance()->resetCounterValue(COUNTER_IDLE);
    V3D::instance()->resetCounterValue(COUNTER_EXECUTIONS);

    // wait some amount
    std::this_thread::sleep_for(std::chrono::seconds{1});

    // read new counter values
    auto qpuIdle = static_cast<float>(V3D::instance()->getCounter(COUNTER_IDLE));
    auto qpuExec = static_cast<float>(V3D::instance()->getCounter(COUNTER_EXECUTIONS));

    if(qpuIdle >= 0 && qpuExec >= 0 && (qpuIdle + qpuExec) > 0)
    {
        // if either is -1, the VC4 is powered down, so it cannot be hung

        // one QPU is 1/12 of full power -> 8%
        // -> to be safe use a threshold of 0.04
        float qpuUsage = qpuExec / (qpuIdle + qpuExec);
        TEST_ASSERT_MSG(qpuUsage < 0.04f, "QPU(s) in a hung state or another program is using them!");
    }
}

void TestExecutions::tear_down()
{
    VC4CL_FUNC(clReleaseCommandQueue)(queue);
    VC4CL_FUNC(clReleaseContext)(context);
    V3D::instance()->disableCounter(COUNTER_IDLE);
    V3D::instance()->disableCounter(COUNTER_EXECUTIONS);
}

void TestExecutions::buildProgram(cl_program* program, const std::string& fileName)
{
    cl_int errcode = CL_SUCCESS;
    sourceFibonacci = readFile(fileName);
    const std::size_t sourceLength = sourceFibonacci.size();
    const char* sourceText = sourceFibonacci.data();
    *program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, &sourceText, &sourceLength, &errcode);
    if(errcode != CL_SUCCESS)
        *program = nullptr;
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(*program != nullptr);
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    errcode = VC4CL_FUNC(clBuildProgram)(*program, 1, &device_id, "", nullptr, nullptr);
    if(errcode != CL_SUCCESS)
        *program = nullptr;
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
}

template <typename R, typename T>
object_wrapper<R> wrap(T* ptr)
{
    object_wrapper<R> wrapper(toType<R>(ptr));
    if(wrapper)
        // Release once, since the object_wrapper constructor retains one additional time, which we do not want
        ignoreReturnValue(wrapper->release(), __FILE__, __LINE__, "Should never occur");
    return wrapper;
}

struct ExecutionRunner final : public test_data::TestRunner
{
    ExecutionRunner(std::unordered_map<std::string, vc4cl::object_wrapper<vc4cl::Program>>& cache, cl_context con,
        cl_command_queue qu) :
        compilationCache(cache),
        context(toType<Context>(con)), queue(toType<CommandQueue>(qu))
    {
    }
    ~ExecutionRunner() noexcept override;

    test_data::Result compile(const std::string& sourceCode, const std::string& options) override
    {
        auto it = compilationCache.find(sourceCode + options);
        if(it != compilationCache.end())
        {
            currentProgram = it->second;
            return test_data::RESULT_OK;
        }
        const char* sourcePtr = sourceCode.data();
        std::size_t sourceLength = sourceCode.size();
        int errcode = CL_SUCCESS;
        currentProgram = wrap<Program>(
            VC4CL_FUNC(clCreateProgramWithSource)(context->toBase(), 1, &sourcePtr, &sourceLength, &errcode));
        if(!currentProgram || errcode != CL_SUCCESS)
            return test_data::Result{false, "Error creating program: " + toErrorMessage(errcode)};

        cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
        errcode = VC4CL_FUNC(clBuildProgram)(
            currentProgram->toBase(), 1, &device_id, options.empty() ? nullptr : options.data(), nullptr, nullptr);
        if(errcode != CL_SUCCESS)
            return test_data::Result{false, "Error compiling program: " + toErrorMessage(errcode)};

        compilationCache.emplace(sourceCode + options, currentProgram);
        return test_data::RESULT_OK;
    }

    test_data::Result selectKernel(const std::string& name) override
    {
        int errcode = CL_SUCCESS;
        argumentBuffers.clear();
        dimensions = {};
        currentKernel = wrap<Kernel>(VC4CL_FUNC(clCreateKernel)(currentProgram->toBase(), name.data(), &errcode));
        if(!currentKernel || errcode != CL_SUCCESS)
            return test_data::Result{false, "Error creating kernel: " + toErrorMessage(errcode)};
        return test_data::RESULT_OK;
    }

    test_data::Result setKernelArgument(
        std::size_t index, bool isLiteral, bool isVector, const void* byteData, std::size_t numBytes) override
    {
        int errcode = CL_SUCCESS;
        if(isLiteral)
            errcode =
                VC4CL_FUNC(clSetKernelArg)(currentKernel->toBase(), static_cast<cl_uint>(index), numBytes, byteData);
        else if(index < currentKernel->info.params.size() && currentKernel->info.params[index].getPointer() &&
            currentKernel->info.params[index].getAddressSpace() == AddressSpace::LOCAL)
            // need special handling for__local buffers, need to pass in NULL
            errcode =
                VC4CL_FUNC(clSetKernelArg)(currentKernel->toBase(), static_cast<cl_uint>(index), numBytes, nullptr);
        else
        {
            // allocate at least 1 word, since a) we read word-sized data via TMU and b) the mailbox also always
            // allocates more data (since data is aligned)
            numBytes = std::max(numBytes, sizeof(uint32_t));
            cl_mem outBuffer = VC4CL_FUNC(clCreateBuffer)(context->toBase(), CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                numBytes, const_cast<void*>(byteData), &errcode);
            if(outBuffer && errcode == CL_SUCCESS)
            {
                if(argumentBuffers.size() <= index)
                    argumentBuffers.resize(index + 1);
                argumentBuffers[index] = wrap<Buffer>(outBuffer);

                errcode = VC4CL_FUNC(clSetKernelArg)(
                    currentKernel->toBase(), static_cast<cl_uint>(index), sizeof(outBuffer), &outBuffer);
            }
        }

        if(errcode != CL_SUCCESS)
            return test_data::Result{false, "Error setting kernel argument:" + toErrorMessage(errcode)};
        return test_data::RESULT_OK;
    }

    test_data::Result setWorkDimensions(const test_data::WorkDimensions& dimensions) override
    {
        this->dimensions = dimensions;
        return test_data::RESULT_OK;
    }

    test_data::Result execute() override
    {
        cl_event event = nullptr;
        std::array<std::size_t, 3> globalOffsets = {
            dimensions.globalOffsets[0],
            dimensions.globalOffsets[1],
            dimensions.globalOffsets[2],
        };
        std::array<std::size_t, 3> globalSizes = {
            dimensions.numGroups[0] * dimensions.localSizes[0],
            dimensions.numGroups[1] * dimensions.localSizes[1],
            dimensions.numGroups[2] * dimensions.localSizes[2],
        };
        std::array<std::size_t, 3> localSizes = {
            dimensions.localSizes[0],
            dimensions.localSizes[1],
            dimensions.localSizes[2],
        };
        auto errcode = VC4CL_FUNC(clEnqueueNDRangeKernel)(queue->toBase(), currentKernel->toBase(),
            dimensions.dimensions, globalOffsets.data(), globalSizes.data(), localSizes.data(), 0, nullptr, &event);
        if(!event || errcode != CL_SUCCESS)
            return test_data::Result{false, "Error queuing kernel execution:" + toErrorMessage(errcode)};
        errcode = VC4CL_FUNC(clWaitForEvents)(1, &event);
        if(errcode != CL_SUCCESS)
            return test_data::Result{false, "Error in kernel execution:" + toErrorMessage(errcode)};
        errcode = VC4CL_FUNC(clReleaseEvent)(event);
        if(errcode != CL_SUCCESS)
            return test_data::Result{false, "Error releasing kernel execution event:" + toErrorMessage(errcode)};
        return test_data::RESULT_OK;
    }

    test_data::Result getKernelArgument(std::size_t index, void* byteData, std::size_t bufferSize) override
    {
        if(index >= argumentBuffers.size())
            return test_data::Result{false, "Argument index is out of bounds"};
        auto& arg = argumentBuffers[index];
        auto errcode = VC4CL_FUNC(clEnqueueReadBuffer)(queue->toBase(), arg->toBase(), CL_TRUE, 0,
            std::min(bufferSize, static_cast<std::size_t>(arg->deviceBuffer->size)), byteData, 0, nullptr, nullptr);
        if(errcode != CL_SUCCESS)
            return test_data::Result{false, "Error reading buffer argument:" + toErrorMessage(errcode)};
        return test_data::RESULT_OK;
    }

    test_data::Result validateFinish() override
    {
        // TODO test for hangs?
        return test_data::RESULT_OK;
    }

    std::string toErrorMessage(int32_t openCLError)
    {
        // TODO proper mapping
        if(currentProgram && currentProgram->buildInfo.status != CL_BUILD_SUCCESS)
            // build error
            return currentProgram->buildInfo.log;
        return std::to_string(openCLError);
    }

    std::unordered_map<std::string, vc4cl::object_wrapper<vc4cl::Program>>& compilationCache;
    object_wrapper<Context> context;
    object_wrapper<CommandQueue> queue;
    object_wrapper<Program> currentProgram;
    object_wrapper<Kernel> currentKernel;
    std::vector<object_wrapper<Buffer>> argumentBuffers;
    test_data::WorkDimensions dimensions;
};

ExecutionRunner::~ExecutionRunner() noexcept = default;

void TestExecutions::runTestData(std::string dataName)
{
    auto test = test_data::getTest(dataName);
    TEST_ASSERT(test != nullptr)
    ExecutionRunner runner{compilationCache, context, queue};
    auto result = test_data::execute(test, runner);
    TEST_ASSERT(result.wasSuccess)
    if(!result.error.empty())
        TEST_ASSERT_EQUALS("(no error)", result.error);
}

void TestExecutions::runNoSuchTestData(std::string dataName)
{
    TEST_ASSERT_EQUALS("(no error)", "There is no test data with the name '" + dataName + "'");
}
