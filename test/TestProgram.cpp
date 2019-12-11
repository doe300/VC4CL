/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestProgram.h"

#include "src/Program.h"
#include "src/icd_loader.h"
#include "util.h"

using namespace vc4cl;

uint32_t hello_world_vector_hex[] = {
#include "hello_world_vector.hex"
};

TestProgram::TestProgram() :
    num_callback(0), num_pendingCallbacks(0), context(nullptr), source_program(nullptr), binary_program(nullptr)
{
    TEST_ADD(TestProgram::testCreateProgramWithSource);
    TEST_ADD(TestProgram::testCreateProgramWithBinary);
    TEST_ADD(TestProgram::testCreateProgramWithBuiltinKernels);
    TEST_ADD(TestProgram::testBuildProgram);
    TEST_ADD(TestProgram::testCompileProgram);
    TEST_ADD(TestProgram::testLinkProgram);
    TEST_ADD(TestProgram::testUnloadPlatformCompiler);
    TEST_ADD(TestProgram::testGetProgramInfo);
    TEST_ADD(TestProgram::testGetProgramBuildInfo);
    TEST_ADD(TestProgram::testRetainProgram);
    TEST_ADD(TestProgram::testReleaseProgram);
}

void TestProgram::checkBuildStatus(const cl_program program)
{
    size_t info_size = 0;
    cl_build_status status = 1;
    cl_int state = VC4CL_FUNC(clGetProgramBuildInfo)(program, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(),
        CL_PROGRAM_BUILD_STATUS, sizeof(status), &status, &info_size);
    if(CL_BUILD_SUCCESS != status)
    {
        // for better error debugging
        std::array<char, 40960> log = {0};
        state = VC4CL_FUNC(clGetProgramBuildInfo)(program, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(),
            CL_PROGRAM_BUILD_LOG, log.size(), log.data(), nullptr);
        std::cerr << "Build log: " << log.data() << std::endl;
        TEST_ASSERT_EQUALS(CL_BUILD_SUCCESS, status)
    }
}

bool TestProgram::setup()
{
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
    return errcode == CL_SUCCESS && context != nullptr;
}

void TestProgram::testCreateProgramWithSource()
{
    cl_int errcode = CL_SUCCESS;
    auto sourceCode = readFile("./test/hello_world_vector.cl");
    const std::size_t sourceLength = sourceCode.size();
    TEST_ASSERT(sourceLength != 0);
    TEST_ASSERT(sourceCode.find("__kernel") != std::string::npos);
    source_program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, nullptr, &sourceLength, &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, source_program);

    const char* strings[1] = {sourceCode.data()};
    source_program = VC4CL_FUNC(clCreateProgramWithSource)(context, 1, strings, &sourceLength, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(source_program != nullptr);
}

void TestProgram::testCreateProgramWithBinary()
{
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    binary_program = VC4CL_FUNC(clCreateProgramWithBinary)(context, 1, &device_id, nullptr, nullptr, nullptr, &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, binary_program);

    cl_int binary_state = CL_SUCCESS;
    size_t binary_size = sizeof(hello_world_vector_hex);
    const unsigned char* strings[1] = {reinterpret_cast<const unsigned char*>(hello_world_vector_hex)};
    binary_program =
        VC4CL_FUNC(clCreateProgramWithBinary)(context, 1, &device_id, &binary_size, strings, &binary_state, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    TEST_ASSERT(binary_program != nullptr);
    TEST_ASSERT_EQUALS(CL_SUCCESS, binary_state);
}

void TestProgram::testCreateProgramWithBuiltinKernels()
{
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    cl_program program = VC4CL_FUNC(clCreateProgramWithBuiltInKernels)(context, 1, &device_id, "", &errcode);
    TEST_ASSERT(errcode != CL_SUCCESS);
    TEST_ASSERT_EQUALS(nullptr, program);
}

struct CallbackData
{
    TestProgram* prog;
    unsigned val;
};

static void build_callback(cl_program prog, void* test)
{
    reinterpret_cast<CallbackData*>(test)->prog->num_callback += reinterpret_cast<CallbackData*>(test)->val;
    --reinterpret_cast<CallbackData*>(test)->prog->num_pendingCallbacks;
}

static void waitForCallback(const std::atomic<unsigned>& counter)
{
    // wait for a limited amount to not loop infinite
    auto remainingTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::seconds{5});
    while(counter != 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{100});
        remainingTime -= std::chrono::milliseconds{100};
        if(remainingTime.count() <= 0)
            return;
    }
}

void TestProgram::testBuildProgram()
{
    CallbackData data{this, 1};
    ++num_pendingCallbacks;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    cl_int state = VC4CL_FUNC(clBuildProgram)(binary_program, 1, &device_id, nullptr, &build_callback, &data);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    // wait until the build is finished
    waitForCallback(num_pendingCallbacks);
    checkBuildStatus(binary_program);
}

void TestProgram::testCompileProgram()
{
    CallbackData data{this, 4};
    ++num_pendingCallbacks;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    cl_int state = VC4CL_FUNC(clCompileProgram)(
        source_program, 1, &device_id, "-Wall", 0, nullptr, nullptr, &build_callback, &data);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    // wait until the build is finished
    waitForCallback(num_pendingCallbacks);
    checkBuildStatus(source_program);
}

void TestProgram::testLinkProgram()
{
    CallbackData data{this, 8};
    ++num_pendingCallbacks;
    cl_int errcode = CL_SUCCESS;
    cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
    cl_program program = VC4CL_FUNC(clLinkProgram)(
        context, 1, &device_id, nullptr, 1, &source_program, &build_callback, &data, &errcode);
    TEST_ASSERT_EQUALS(CL_SUCCESS, errcode);
    // clLinkProgram is specified to create a new program object
    TEST_ASSERT(source_program != program);

    VC4CL_FUNC(clReleaseProgram)(source_program);
    // this pointer is used for further access
    source_program = program;
    // wait until the build is finished
    waitForCallback(num_pendingCallbacks);
    checkBuildStatus(program);
}

void TestProgram::testUnloadPlatformCompiler()
{
    cl_int state = VC4CL_FUNC(clUnloadPlatformCompiler)(nullptr);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    state = VC4CL_FUNC(clUnloadCompiler)();
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    // 1 + 4 + 8 = 13
    // The callback is only fired once in #testBuildProgram(), since the program is already compiled (and only need to
    // be linked)
    TEST_ASSERT_EQUALS(13u, num_callback);
}

void TestProgram::testGetProgramBuildInfo()
{
    size_t info_size = 0;
    char buffer[2048];
    cl_int state = VC4CL_FUNC(clGetProgramBuildInfo)(source_program,
        Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), CL_PROGRAM_BUILD_STATUS, 2048, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_build_status), info_size);
    TEST_ASSERT_EQUALS(CL_BUILD_SUCCESS, *reinterpret_cast<cl_build_status*>(buffer));

    state = VC4CL_FUNC(clGetProgramBuildInfo)(source_program, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(),
        CL_PROGRAM_BUILD_OPTIONS, 2048, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(info_size > 0u);

    state = VC4CL_FUNC(clGetProgramBuildInfo)(source_program, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(),
        CL_PROGRAM_BUILD_LOG, 2048, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(info_size > 0u);

    state = VC4CL_FUNC(clGetProgramBuildInfo)(source_program, Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(),
        CL_PROGRAM_BINARY_TYPE, 2048, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_program_binary_type), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_program_binary_type>(CL_PROGRAM_BINARY_TYPE_EXECUTABLE),
        *reinterpret_cast<cl_program_binary_type*>(buffer));
}

void TestProgram::testGetProgramInfo()
{
    size_t info_size = 0;
    char buffer[2048];
    cl_int state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_REFERENCE_COUNT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_CONTEXT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_context), info_size);
    TEST_ASSERT_EQUALS(context, *reinterpret_cast<cl_context*>(buffer));

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_NUM_DEVICES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_DEVICES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_id), info_size);
    TEST_ASSERT_EQUALS(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), *reinterpret_cast<cl_device_id*>(buffer));

    // XXX not valid anymore, source_program is not the original source_program
    // state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_SOURCE, 128, buffer, &info_size);
    // TEST_ASSERT_EQUALS(CL_INVALID_VALUE, state);   //buffer-size!
    // TEST_ASSERT(info_size > 0u);

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_SOURCE, 2048, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(info_size > 0u && info_size < 2048u);

    state = VC4CL_FUNC(clGetProgramInfo)(binary_program, CL_PROGRAM_SOURCE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, info_size);

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_BINARY_SIZES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);

    auto ptr = reinterpret_cast<void*>(buffer);
    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_BINARIES, 1024, &ptr, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_NUM_KERNELS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<size_t*>(buffer));

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, CL_PROGRAM_KERNEL_NAMES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    state = VC4CL_FUNC(clGetProgramInfo)(source_program, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT(state != CL_SUCCESS);
}

void TestProgram::testRetainProgram()
{
    TEST_ASSERT_EQUALS(1u, toType<Program>(source_program)->getReferences());
    cl_int state = VC4CL_FUNC(clRetainProgram)(source_program);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT_EQUALS(2u, toType<Program>(source_program)->getReferences());
    state = VC4CL_FUNC(clReleaseProgram)(source_program);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, toType<Program>(source_program)->getReferences());
}

void TestProgram::testReleaseProgram()
{
    TEST_ASSERT_EQUALS(1u, toType<Program>(source_program)->getReferences());
    cl_int state = VC4CL_FUNC(clReleaseProgram)(source_program);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    TEST_ASSERT_EQUALS(1u, toType<Program>(binary_program)->getReferences());
    state = VC4CL_FUNC(clReleaseProgram)(binary_program);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestProgram::tear_down()
{
    VC4CL_FUNC(clReleaseContext)(context);
}
