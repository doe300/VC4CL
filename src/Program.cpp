/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Program.h"

#include "Device.h"
#include "V3D.h"
#include "extensions.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <thread>

#ifdef COMPILER_HEADER
#define CPPLOG_NAMESPACE logging
#include COMPILER_HEADER
#endif

using namespace vc4cl;

size_t KernelInfo::getExplicitUniformCount() const
{
    size_t count = 0;
    for(const ParamInfo& info : params)
        count += info.getVectorElements();
    return count;
}

Program::Program(Context* context, const std::vector<char>& code, CreationType type) :
    HasContext(context), creationType(type)
{
    switch(type)
    {
    case CreationType::SOURCE:
        sourceCode = code;
        break;
    case CreationType::INTERMEDIATE_LANGUAGE:
    case CreationType::LIBRARY:
        intermediateCode.resize(code.size() / sizeof(uint8_t), '\0');
        if(!code.empty())
            memcpy(intermediateCode.data(), code.data(), code.size());
        break;
    case CreationType::BINARY:
        binaryCode.resize(code.size() / sizeof(uint64_t), '\0');
        if(!code.empty())
            memcpy(binaryCode.data(), code.data(), code.size());
    }
}

Program::~Program() noexcept = default;

#if HAS_COMPILER
static cl_int extractLog(std::string& log, std::wstringstream& logStream)
{
    /*
     * this method is not supported by the Raspbian GCC:
     * std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> logConverter;
     * program->buildInfo.log = logConverter.to_bytes(logStream.str());
     */
    //"POSIX specifies a common extension: if dst is a null pointer, this function returns the number of bytes that
    // would be written to dst, if converted."
    std::size_t numCharacters = std::wcstombs(nullptr, logStream.str().data(), SIZE_MAX);
    //"On conversion error (if invalid wide character was encountered), returns static_cast<std::size_t>(-1)."
    if(numCharacters == static_cast<std::size_t>(-1))
        return returnError(CL_BUILD_ERROR, __FILE__, __LINE__, "Invalid character sequence in build-log");
    else
    {
        std::vector<char> logTmp(numCharacters + 1 /* \0 byte */);
        numCharacters = std::wcstombs(logTmp.data(), logStream.str().data(), numCharacters);
        log.append(logTmp.data(), numCharacters);
    }

    return CL_SUCCESS;
}

static cl_int precompile_program(Program* program, const std::string& options,
    const std::unordered_map<std::string, object_wrapper<Program>>& embeddedHeaders)
{
    std::istringstream sourceCode;
    // to avoid the warning about "null character ignored"
    auto length = program->sourceCode.size();
    if(length > 0 && program->sourceCode.back() == '\0')
        --length;
    sourceCode.str(std::string(program->sourceCode.data(), length));

    vc4c::SourceType sourceType = vc4c::Precompiler::getSourceType(sourceCode);
    if(sourceType == vc4c::SourceType::UNKNOWN || sourceType == vc4c::SourceType::QPUASM_BIN ||
        sourceType == vc4c::SourceType::QPUASM_HEX)
    {
        program->buildInfo.log.append("Invalid source-code type:");
        program->buildInfo.log.append(program->sourceCode.data(), length);
        return returnError(
            CL_COMPILE_PROGRAM_FAILURE, __FILE__, __LINE__, buildString("Invalid source-code type %d", sourceType));
    }

    vc4c::Configuration config;

    program->buildInfo.options = options;
    DEBUG_LOG(DebugLevel::DUMP_CODE, {
        std::cout << "Precompiling source with: " << program->buildInfo.options << std::endl;
        const std::string dumpFile("/tmp/vc4cl-source-" + std::to_string(rand()) + ".cl");
        std::cout << "Dumping program sources to " << dumpFile << std::endl;
        std::ofstream f(dumpFile, std::ios_base::out | std::ios_base::trunc);
        f << sourceCode.str();
        f.close();
    })

    cl_int status = CL_SUCCESS;
    std::wstringstream logStream;
    try
    {
        vc4c::setLogger(logStream, false, vc4c::LogLevel::WARNING);
        // create temporary files for embedded headers and include their paths
        std::vector<vc4c::TemporaryFile> tempHeaderFiles;
        tempHeaderFiles.reserve(embeddedHeaders.size());
        std::string tempHeaderIncludes;
        for(const auto& pair : embeddedHeaders)
        {
            // TODO sub-folders
            tempHeaderFiles.emplace_back(std::string("/tmp/") + pair.first, pair.second->sourceCode);
        }
        if(!tempHeaderFiles.empty())
            tempHeaderIncludes = " -I /tmp/ ";

        vc4c::TemporaryFile tmpFile;
        std::unique_ptr<std::istream> out;
        vc4c::Precompiler::precompile(sourceCode, out, config, tempHeaderIncludes + options, {}, tmpFile.fileName);
        if(out == nullptr ||
            (dynamic_cast<std::istringstream*>(out.get()) != nullptr &&
                dynamic_cast<std::istringstream*>(out.get())->str().empty()))
            // replace only when pre-compiled (and not just linked output to input, e.g. if source-type is output-type)
            tmpFile.openInputStream(out);

        uint8_t tmp;
        while(out->read(reinterpret_cast<char*>(&tmp), sizeof(uint8_t)))
            program->intermediateCode.push_back(tmp);

        DEBUG_LOG(DebugLevel::DUMP_CODE, {
            // reset the tmp buffer, so we can actually read from it again
            out->clear();
            out->seekg(0);
            auto irType = vc4c::Precompiler::getSourceType(*out);
            bool isSPIRVType = irType == vc4c::SourceType::SPIRV_BIN || irType == vc4c::SourceType::SPIRV_TEXT;
            const std::string dumpFile("/tmp/vc4cl-ir-" + std::to_string(rand()) + (isSPIRVType ? ".spt" : ".ll"));
            std::cout << "Dumping program IR to " << dumpFile << std::endl;
            vc4c::Precompiler precomp(config, *out, irType);
            std::unique_ptr<std::istream> irOut;
            precomp.run(
                irOut, isSPIRVType ? vc4c::SourceType::SPIRV_TEXT : vc4c::SourceType::LLVM_IR_TEXT, "", dumpFile);
        })
    }
    catch(vc4c::CompilationError& e)
    {
        DEBUG_LOG(DebugLevel::DUMP_CODE, std::cout << "Precompilation error: " << e.what() << std::endl)

        program->buildInfo.log.append("Precompilation error:\n\t").append(e.what()).append("\n");
        status = CL_COMPILE_PROGRAM_FAILURE;
    }
    // copy log whether build failed or not
    extractLog(program->buildInfo.log, logStream);

    DEBUG_LOG(DebugLevel::DUMP_CODE, {
        std::cout << "Precompilation complete with status: " << status << std::endl;
        if(!program->buildInfo.log.empty())
            std::cout << "Compilation log: " << program->buildInfo.log << std::endl;
    })

    return status;
}

static cl_int link_programs(
    Program* program, const std::vector<object_wrapper<Program>>& otherPrograms, bool includeStandardLibrary)
{
    if(otherPrograms.empty() && !includeStandardLibrary)
        return CL_SUCCESS;

    cl_int status = CL_SUCCESS;
    std::wstringstream logStream;
    try
    {
        vc4c::setLogger(logStream, false, vc4c::LogLevel::WARNING);

        std::stringstream linkedCode;
        std::unordered_map<std::istream*, vc4c::Optional<std::string>> inputModules;
        std::vector<std::unique_ptr<std::istream>> streamsBuffer;
        streamsBuffer.reserve(1 + otherPrograms.size());
        if(!program->intermediateCode.empty())
        {
            streamsBuffer.emplace_back(new std::stringstream(std::string(
                reinterpret_cast<const char*>(program->intermediateCode.data()), program->intermediateCode.size())));
            inputModules.emplace(streamsBuffer.back().get(), vc4c::Optional<std::string>{});
        }
        for(const auto& p : otherPrograms)
        {
            if(p && !p->intermediateCode.empty())
            {
                streamsBuffer.emplace_back(new std::stringstream(std::string(
                    reinterpret_cast<const char*>(p->intermediateCode.data()), p->intermediateCode.size())));
                inputModules.emplace(streamsBuffer.back().get(), vc4c::Optional<std::string>{});
            }
        }
        if(!vc4c::Precompiler::isLinkerAvailable(inputModules))
            return returnError(
                CL_LINKER_NOT_AVAILABLE, __FILE__, __LINE__, "No linker available for this type of input modules!");
        vc4c::Precompiler::linkSourceCode(inputModules, linkedCode, includeStandardLibrary);
        program->intermediateCode.clear();
        uint8_t tmp;
        while(linkedCode.read(reinterpret_cast<char*>(&tmp), sizeof(uint8_t)))
            program->intermediateCode.push_back(tmp);

        DEBUG_LOG(DebugLevel::DUMP_CODE, {
            // reset the tmp buffer, so we can actually read from it again
            linkedCode.clear();
            linkedCode.seekg(0);
            auto irType = vc4c::Precompiler::getSourceType(linkedCode);
            bool isSPIRVType = irType == vc4c::SourceType::SPIRV_BIN || irType == vc4c::SourceType::SPIRV_TEXT;
            const std::string dumpFile("/tmp/vc4cl-ir-" + std::to_string(rand()) + (isSPIRVType ? ".spt" : ".ll"));
            std::cout << "Dumping program IR to " << dumpFile << std::endl;
            vc4c::Configuration dummyConfig{};
            vc4c::Precompiler precomp(dummyConfig, linkedCode, irType);
            std::unique_ptr<std::istream> irOut;
            precomp.run(
                irOut, isSPIRVType ? vc4c::SourceType::SPIRV_TEXT : vc4c::SourceType::LLVM_IR_TEXT, "", dumpFile);
        })
    }
    catch(vc4c::CompilationError& e)
    {
        DEBUG_LOG(DebugLevel::DUMP_CODE, std::cout << "Link error: " << e.what() << std::endl)

        program->buildInfo.log.append("Link error:\n\t").append(e.what()).append("\n");
        status = CL_LINK_PROGRAM_FAILURE;
    }
    // copy log whether build failed or not
    extractLog(program->buildInfo.log, logStream);

    DEBUG_LOG(DebugLevel::DUMP_CODE, {
        std::cout << "Linking complete with status: " << status << std::endl;
        if(!program->buildInfo.log.empty())
            std::cout << "Compilation log: " << program->buildInfo.log << std::endl;
    })

    return status;
}

static cl_int compile_program(Program* program, const std::string& options)
{
    std::stringstream intermediateCode;
    intermediateCode.write(reinterpret_cast<char*>(program->intermediateCode.data()),
        static_cast<std::streamsize>(program->intermediateCode.size()));

    vc4c::SourceType sourceType = vc4c::Precompiler::getSourceType(intermediateCode);
    if(sourceType == vc4c::SourceType::UNKNOWN || sourceType == vc4c::SourceType::QPUASM_BIN ||
        sourceType == vc4c::SourceType::QPUASM_HEX)
        return returnError(
            CL_BUILD_PROGRAM_FAILURE, __FILE__, __LINE__, buildString("Invalid source-code type %d", sourceType));

    vc4c::Configuration config;
    // set the configuration for the available VPM size
    /*
     * NOTE: VC4 OpenGL driver disables the VPM user-memory completely
     * (see https://github.com/raspberrypi/linux/blob/rpi-4.9.y/drivers/gpu/drm/vc4/vc4_v3d.c #vc4_v3d_init_hw)
     */
    config.availableVPMSize = V3D::instance()->getSystemInfo(SystemInfo::VPM_MEMORY_SIZE);

    program->buildInfo.options = options;
    DEBUG_LOG(DebugLevel::DUMP_CODE, std::cout << "Compiling source with: " << program->buildInfo.options << std::endl)

    cl_int status = CL_SUCCESS;
    std::wstringstream logStream;
    try
    {
        vc4c::setLogger(logStream, false, vc4c::LogLevel::WARNING);

        std::stringstream binaryCode;
        std::size_t numBytes = vc4c::Compiler::compile(intermediateCode, binaryCode, config, options);
        program->binaryCode.resize(numBytes / sizeof(uint64_t), '\0');

        memcpy(program->binaryCode.data(), binaryCode.str().data(), numBytes);
    }
    catch(vc4c::CompilationError& e)
    {
        DEBUG_LOG(DebugLevel::DUMP_CODE, std::cout << "Compilation error: " << e.what() << std::endl)

        program->buildInfo.log.append("Compilation error:\n\t").append(e.what()).append("\n");
        status = CL_BUILD_PROGRAM_FAILURE;
    }
    // copy log whether build failed or not
    extractLog(program->buildInfo.log, logStream);

    DEBUG_LOG(DebugLevel::DUMP_CODE, {
        std::cout << "Compilation complete with status: " << status << std::endl;
        if(!program->buildInfo.log.empty())
        {
            std::cout << "Compilation log: " << program->buildInfo.log << std::endl;
        }
        const std::string dumpFile("/tmp/vc4cl-binary-" + std::to_string(rand()) + ".bin");
        std::cout << "Dumping program binaries to " << dumpFile << std::endl;
        std::ofstream f(dumpFile, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
        f.write(reinterpret_cast<char*>(program->binaryCode.data()),
            static_cast<std::streamsize>(program->binaryCode.size() * sizeof(uint64_t)));
        f.close();
    })

    return status;
}

#endif

cl_int Program::compile(
    const std::string& options, const std::unordered_map<std::string, object_wrapper<Program>>& embeddedHeaders)
{
    if(sourceCode.empty())
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "There is no source code to compile!");
    // if the program was already compiled, clear all results
    intermediateCode.clear();
    binaryCode.clear();
    moduleInfo.kernelInfos.clear();
#if HAS_COMPILER
    cl_int state = precompile_program(this, options, embeddedHeaders);
#else
    buildInfo.status = CL_BUILD_NONE;
    cl_int state = CL_COMPILER_NOT_AVAILABLE;
#endif
    return state;
}

cl_int Program::link(const std::string& options, const std::vector<object_wrapper<Program>>& programs)
{
    cl_int status = CL_SUCCESS;
#if HAS_COMPILER
    if(binaryCode.empty())
    {
        // the actual link step
        if(intermediateCode.empty() && programs.empty())
            // not yet compiled. Don't check, if the other programs have intermediate code (this is the output-program
            // for linking)
            return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Program needs to be compiled first!");

        // link and compile program(s)
        // if the program is created from source, the standard-library has already been linked in
        // if the program is created from machine code, this is never called
        status = link_programs(this, programs, creationType == CreationType::INTERMEDIATE_LANGUAGE);

        if(status == CL_SUCCESS && creationType != CreationType::LIBRARY)
            /*
             * If we create a library, don't compile it to machine code yet, leave it as intermediate code.
             * This is okay, since a library on its own has to be linked again into another program anyway to be useful.
             */
            status = compile_program(this, options);
    }

    // extract kernel-info
    if(status == CL_SUCCESS && creationType != CreationType::LIBRARY)
    {
        // if the program was already compiled, clear all results
        moduleInfo.kernelInfos.clear();
        status = extractModuleInfo();
    }
#else
    buildInfo.status = CL_BUILD_NONE;
    status = CL_COMPILER_NOT_AVAILABLE;
#endif
    return status;
}

cl_int Program::getInfo(
    cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    std::string kernelNames;
    for(const KernelInfo& info : moduleInfo.kernelInfos)
    {
        kernelNames.append(info.name).append(";");
    }
    // remove last semicolon
    kernelNames = kernelNames.substr(0, kernelNames.length() - 1);

    switch(param_name)
    {
    case CL_PROGRAM_REFERENCE_COUNT:
        return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_CONTEXT:
        return returnValue<cl_context>(context()->toBase(), param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_NUM_DEVICES:
        return returnValue<cl_uint>(1, param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_DEVICES:
        return returnValue<cl_device_id>(
            Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_IL_KHR:
        //"Returns the program IL for programs created with clCreateProgramWithILKHR"
        if(creationType != CreationType::INTERMEDIATE_LANGUAGE)
            //"[...] the memory pointed to by param_value will be unchanged and param_value_size_ret will be set to
            // zero."
            return returnValue(nullptr, 0, 0, param_value_size, param_value, param_value_size_ret);
        return returnValue(intermediateCode.data(), sizeof(uint8_t), intermediateCode.size(), param_value_size,
            param_value, param_value_size_ret);
    case CL_PROGRAM_SOURCE:
        if(sourceCode.empty() || creationType != CreationType::SOURCE)
            //"[..] a null string or the appropriate program source code is returned [...] "
            return returnString("", param_value_size, param_value, param_value_size_ret);
        return returnValue(
            sourceCode.data(), sizeof(char), sourceCode.size(), param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_BINARY_SIZES:
        if(binaryCode.empty())
            /*
             * "[...] could be an executable binary, compiled binary or library binary [...]"
             * Executable binaries are stored in binaryData, compiled binaries in intermediateCode
             */
            return returnValue<size_t>(
                intermediateCode.size() * sizeof(uint8_t), param_value_size, param_value, param_value_size_ret);
        return returnValue<size_t>(
            binaryCode.size() * sizeof(uint64_t), param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_BINARIES:
        //"param_value points to an array of n pointers allocated by the caller, where n is the number of devices
        // associated with program. "
        if(binaryCode.empty())
        {
            /*
             * "[...] could be an executable binary, compiled binary or library binary [...]"
             * Executable binaries are stored in binaryData, compiled binaries in intermediateCode
             */
            if(intermediateCode.empty())
                return returnValue(nullptr, 0, 0, param_value_size, param_value, param_value_size_ret);
            return returnBuffers({intermediateCode.data()}, {intermediateCode.size() * sizeof(uint8_t)},
                sizeof(unsigned char*), param_value_size, param_value, param_value_size_ret);
        }
        return returnBuffers({reinterpret_cast<uint8_t*>(binaryCode.data())}, {binaryCode.size() * sizeof(uint64_t)},
            sizeof(unsigned char*), param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_NUM_KERNELS:
        if(moduleInfo.kernelInfos.empty())
            return CL_INVALID_PROGRAM_EXECUTABLE;
        return returnValue<size_t>(moduleInfo.kernelInfos.size(), param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_KERNEL_NAMES:
        if(moduleInfo.kernelInfos.empty())
            return CL_INVALID_PROGRAM_EXECUTABLE;
        return returnString(kernelNames, param_value_size, param_value, param_value_size_ret);
    }

    return returnError(
        CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_program_info value %d", param_name));
}

cl_int Program::getBuildInfo(
    cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    switch(param_name)
    {
    case CL_PROGRAM_BUILD_STATUS:
        return returnValue<cl_build_status>(buildInfo.status, param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_BUILD_OPTIONS:
        return returnString(buildInfo.options, param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_BUILD_LOG:
        return returnString(buildInfo.log, param_value_size, param_value, param_value_size_ret);
    case CL_PROGRAM_BINARY_TYPE:
        // only the source-code is set
        if(binaryCode.empty() && intermediateCode.empty())
            return returnValue<cl_program_binary_type>(
                CL_PROGRAM_BINARY_TYPE_NONE, param_value_size, param_value, param_value_size_ret);
        // the intermediate code is set, but the final code is not -> not yet linked
        // XXX for programs loaded via SPIR input (cl_khr_spir), need to return CL_PROGRAM_BINARY_TYPE_INTERMEDIATE here
        if(binaryCode.empty())
        {
            cl_program_binary_type type = creationType == CreationType::LIBRARY ?
                CL_PROGRAM_BINARY_TYPE_LIBRARY :
                CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT;
            return returnValue<cl_program_binary_type>(type, param_value_size, param_value, param_value_size_ret);
        }
        return returnValue<cl_program_binary_type>(
            CL_PROGRAM_BINARY_TYPE_EXECUTABLE, param_value_size, param_value, param_value_size_ret);
    }

    return returnError(
        CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_program_build_info value %d", param_name));
}

BuildStatus Program::getBuildStatus() const
{
    if(binaryCode.empty() && intermediateCode.empty())
        return BuildStatus::NOT_BUILD;
    if(binaryCode.empty())
        return BuildStatus::COMPILED;
    return BuildStatus::DONE;
}

static std::string readString(cl_ulong** ptr, cl_uint stringLength)
{
    const std::string s(reinterpret_cast<char*>(*ptr), stringLength);
    *ptr += stringLength / 8;
    if(stringLength % 8 != 0)
    {
        *ptr += 1;
    }
    return s;
}

cl_int Program::extractModuleInfo()
{
    cl_ulong* ptr = reinterpret_cast<cl_ulong*>(binaryCode.data());
    // check and skip magic number
    if(*reinterpret_cast<const cl_uint*>(ptr) != kernel_config::BINARY_MAGIC_NUMBER)
        return returnError(
            CL_INVALID_BINARY, __FILE__, __LINE__, "Invalid binary data given, magic number does not match!");
    ptr += 1;

    // read and skip module info
    moduleInfo = ModuleInfo(*ptr);
    ptr += 1;
    while(moduleInfo.kernelInfos.size() < moduleInfo.getInfoCount())
    {
        cl_int state = extractKernelInfo(&ptr);
        if(state != CL_SUCCESS)
        {
            moduleInfo.kernelInfos.clear();
            return state;
        }
    }

    if(moduleInfo.kernelInfos.empty())
        // no kernel meta-data was found!
        return returnError(CL_INVALID_PROGRAM, __FILE__, __LINE__, "No kernel offset found!");

    if(moduleInfo.getGlobalDataSize() > 0)
    {
        uint64_t* globalsPtr = &binaryCode[moduleInfo.getGlobalDataOffset()];
        globalData.reserve(moduleInfo.getGlobalDataSize());
        std::copy(globalsPtr, globalsPtr + moduleInfo.getGlobalDataSize(), std::back_inserter(globalData));
    }

    return CL_SUCCESS;
}

cl_int Program::extractKernelInfo(cl_ulong** ptr)
{
    KernelInfo info(*reinterpret_cast<uint64_t*>(*ptr));
    *ptr += 1;

    info.compileGroupSizes[0] = **ptr & 0xFFFF;
    info.compileGroupSizes[1] = (**ptr >> 16) & 0xFFFF;
    info.compileGroupSizes[2] = (**ptr >> 32) & 0xFFFF;
    *ptr += 1;

    info.uniformsUsed.value = *reinterpret_cast<uint64_t*>(*ptr);
    *ptr += 1;

    // name[...]|padding
    info.name = readString(ptr, info.getNameLength());

    for(cl_ushort i = 0; i < info.getParamCount(); ++i)
    {
        ParamInfo param(*reinterpret_cast<uint64_t*>(*ptr));
        *ptr += 1;

        param.name = readString(ptr, param.getNameLength());
        param.type = readString(ptr, param.getTypeNameLength());

        info.params.push_back(param);
    }

    moduleInfo.kernelInfos.push_back(info);

    return CL_SUCCESS;
}

using BuildCallback = void(CL_CALLBACK*)(cl_program program, void* user_data);

static cl_int buildInner(object_wrapper<Program> program, std::string options, BuildCallback callback, void* userData)
{
    cl_int state = CL_SUCCESS;
    if(program->getBuildStatus() != BuildStatus::COMPILED && !program->sourceCode.empty())
        // if the program was never build, compile. If it was already built once, re-compile (only if original
        // source is available)  since clCompileProgram overwrites the build-status, we can't call it, instead
        // directly call Program#compile
        state = program->compile(options, std::unordered_map<std::string, object_wrapper<Program>>{});
    if(state == CL_SUCCESS)
        // don't call clLinkProgram, since it creates a new program, while clBuildProgram does not
        state = program->link(options, {});

    if(state != CL_SUCCESS)
        program->buildInfo.status = CL_BUILD_ERROR;
    else
        program->buildInfo.status = CL_BUILD_SUCCESS;
    if(callback)
        (*callback)(program->toBase(), userData);
    return state;
};

static cl_int compileInner(object_wrapper<Program> program, std::string options,
    std::unordered_map<std::string, object_wrapper<Program>> embeddedHeaders, BuildCallback callback, void* userData)
{
    cl_int state = program->compile(options, embeddedHeaders);
    if(state != CL_SUCCESS)
        program->buildInfo.status = CL_BUILD_ERROR;
    else
        program->buildInfo.status = CL_BUILD_SUCCESS;
    if(callback)
        (callback)(program->toBase(), userData);
    return state;
};

static cl_int linkInner(object_wrapper<Program> program, std::string options,
    std::vector<object_wrapper<Program>> inputPrograms, BuildCallback callback, void* userData)
{
    cl_int status = program->link(options, inputPrograms);
    if(status != CL_SUCCESS)
        program->buildInfo.status = CL_BUILD_ERROR;
    else
        program->buildInfo.status = CL_BUILD_SUCCESS;
    if(callback)
        (*callback)(program->toBase(), userData);
    return status;
};

/*!
 * OpenCL 1.2 specification, pages 133+:
 *
 *  Creates a program object for a context, and loads the source code specified by the text strings in the strings array
 * into the program object. The devices associated with the program object are the devices associated with context. The
 * source code specified by strings is either an OpenCL C program source, header or implementation-defined source for
 * custom devices that support an online compiler.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param strings is an array of count pointers to optionally null-terminated character strings that make up the source
 * code.
 *
 *  \param The lengths argument is an array with the number of chars in each string (the string length). If an element
 * in lengths is zero, its accompanying string is null-terminated. If lengths is NULL , all strings in the strings
 * argument are considered null-terminated. Any length value passed in that is greater than zero excludes the null
 * terminator in its count.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \retrun clCreateProgramWithSource returns a valid non-zero program object and errcode_ret is set to CL_SUCCESS if
 * the program object is created successfully. Otherwise, it returns a NULL value with one of the following error values
 * returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if count is zero or if strings or any entry in strings is NULL .
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_program VC4CL_FUNC(clCreateProgramWithSource)(
    cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_program", clCreateProgramWithSource, "cl_context", context, "cl_uint", count,
        "const char**", strings, "const size_t*", lengths, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)

    if(count == 0 || strings == nullptr)
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No source given!");
    for(cl_uint i = 0; i < count; ++i)
    {
        if(strings[i] == nullptr)
            return returnError<cl_program>(
                CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Source code line is NULL!");
    }

    std::vector<char> sourceCode;

    size_t code_length = 0;
    for(cl_uint i = 0; i < count; ++i)
    {
        size_t line_length = 0;
        if(lengths == nullptr || lengths[i] == 0)
            line_length = strlen(strings[i]);
        else
            line_length = lengths[i];
        code_length += line_length;
    }
    sourceCode.reserve(code_length + 1);

    for(cl_uint i = 0; i < count; ++i)
    {
        size_t line_length = 0;
        if(lengths == nullptr || lengths[i] == 0)
            line_length = strlen(strings[i]);
        else
            line_length = lengths[i];
        std::copy(strings[i], strings[i] + line_length, std::back_inserter(sourceCode));
    }
    sourceCode.push_back('\0');

    Program* program = newOpenCLObject<Program>(toType<Context>(context), sourceCode, CreationType::SOURCE);
    CHECK_ALLOCATION_ERROR_CODE(program, errcode_ret, cl_program)

    RETURN_OBJECT(program->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 extensions specification, pages 155+:
 *
 *  Creates a new program object for context using the length bytes of intermediate language pointed to by il.
 *
 *  \param context must be a valid OpenCL context
 *
 *  \param il is a pointer to a length-byte block of memory containing intermediate language.
 *
 *  \param length is the length of the block of memory pointed to by il.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateProgramWithILKHR returns a valid non-zero program object and errcode_ret is set to CL_SUCCESS if the
 * program object is created successfully. Otherwise, it returns a NULL value with one of the following error values
 * returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context
 *  - CL_INVALID_VALUE if il is NULL or if length is zero
 *  - CL_INVALID_VALUE if the length-byte block of memory pointed to by il does not contain well-formed intermediate
 * language
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host
 */
cl_program VC4CL_FUNC(clCreateProgramWithILKHR)(cl_context context, const void* il, size_t length, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_program", clCreateProgramWithILKHR, "cl_context", context, "const void*", il, "size_t",
        length, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)
    if(il == nullptr || length == 0)
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "IL source has no length!");

    const std::vector<char> buffer(static_cast<const char*>(il), static_cast<const char*>(il) + length);
    Program* program = newOpenCLObject<Program>(toType<Context>(context), buffer, CreationType::INTERMEDIATE_LANGUAGE);
    CHECK_ALLOCATION_ERROR_CODE(program, errcode_ret, cl_program)

    RETURN_OBJECT(program->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pages 134+:
 *
 *  Creates a program object for a context, and loads the binary bits specified by binary into the program object.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param device_list is a pointer to a list of devices that are in context. device_list must be a non-NULL value.
 *  The binaries are loaded for devices specified in this list.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  The devices associated with the program object will be the list of devices specified by device_list. The list of
 * devices specified by device_list must be devices associated with context.
 *
 *  \param lengths is an array of the size in bytes of the program binaries to be loaded for devices specified by
 * device_list.
 *
 *  \param binaries is an array of pointers to program binaries to be loaded for devices specified by device_list.
 *  For each device given by device_list[i], the pointer to the program binary for that device is given by binaries[i]
 * and the length of this corresponding binary is given by lengths[i]. lengths[i] cannot be zero and binaries[i] cannot
 * be a NULL pointer. The program binaries specified by binaries contain the bits that describe one of the following:
 *   - a program executable to be run on the device(s) associated with context,
 *   - a compiled program for device(s) associated with context, or
 *   - a library of compiled programs for device(s) associated with context.
 *  The program binary can consist of either or both:
 *   - Device-specific code and/or,
 *   - Implementation-specific intermediate representation (IR) which will be converted to th edevice-specific code.
 *
 *  \param binary_status returns whether the program binary for each device specified in device_list was loaded
 * successfully or not. It is an array of num_devices entries and returns CL_SUCCESS in binary_status[i] if binary was
 * successfully loaded for device specified by device_list[i]; otherwise returns CL_INVALID_VALUE if lengths[i] is zero
 * or if binaries[i] is a NULL value or CL_INVALID_BINARY in binary_status[i] if program binary is not a valid binary
 * for the specified device. If binary_status is NULL , it is ignored.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clCreateProgramWithBinary returns a valid non-zero program object and errcode_ret is set to CL_SUCCESS if
 * the program object is created successfully. Otherwise, it returns a NULL value with one of the following error values
 * returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if device_list is NULL or num_devices is zero.
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with context.
 *  - CL_INVALID_VALUE if lengths or binaries are NULL or if any entry in lengths[i] is zero or binaries[i] is NULL .
 *  - CL_INVALID_BINARY if an invalid program binary was encountered for any device. binary_status will return specific
 * status for each device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  OpenCL allows applications to create a program object using the program source or binary and build appropriate
 * program executables. This can be very useful as it allows applications to load program source and then compile and
 * link to generate a program executable online on its first instance for appropriate OpenCL devices in the system.
 * These executables can now be queried and cached by the application. Future instances of the application launching
 * will no longer need to compile and link the program executables. The cached executables can be read and loaded by
 * the application, which can help significantly reduce the application initialization time.
 *
 */
cl_program VC4CL_FUNC(clCreateProgramWithBinary)(cl_context context, cl_uint num_devices,
    const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status,
    cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_program", clCreateProgramWithBinary, "cl_context", context, "cl_uint", num_devices,
        "const cl_device_id*", device_list, "const size_t*", lengths, "const unsigned char**", binaries, "cl_int*",
        binary_status, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)

    if(num_devices == 0 || device_list == nullptr)
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No devices specified!");

    if(num_devices > 1)
        // only 1 device supported
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
            "Multiple devices specified, only a single is supported!");

    if(device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
        return returnError<cl_program>(
            CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__, "Device specified is not the VC4CL GPU device!");

    if(lengths == nullptr || binaries == nullptr)
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No binary data given!");
    if(lengths[0] == 0 || binaries[0] == nullptr)
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Empty binary data given!");

    /*
     * OpenCL 1.2 extension specification page 135 for cl_khr_spir states:
     * "clCreateProgramWithBinary can be used to load a SPIR binary."
     * -> so if the check for a valid QPU binary fails, assume SPIR binary
     */
    bool isValidQPUCode = *reinterpret_cast<const cl_uint*>(binaries[0]) == kernel_config::BINARY_MAGIC_NUMBER;
    const std::vector<char> buffer(binaries[0], binaries[0] + lengths[0]);
    Program* program = newOpenCLObject<Program>(
        toType<Context>(context), buffer, isValidQPUCode ? CreationType::BINARY : CreationType::INTERMEDIATE_LANGUAGE);
    CHECK_ALLOCATION_ERROR_CODE(program, errcode_ret, cl_program)

    if(binary_status != nullptr)
        binary_status[0] = CL_SUCCESS;

    RETURN_OBJECT(program->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pages 136+:
 *
 *  Creates a program object for a context, and loads the information related to the built-in kernels into a program
 * object.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param device_list is a pointer to a list of devices that are in context. device_list must be a non-NULL value.
 *  The built-in kernels are loaded for devices specified in this list. The devices associated with the program object
 * will be the list of devices specified by device_list. The list of devices specified by device_list must be devices
 * associated with context.
 *
 *  \param kernel_names is a semi-colon separated list of built-in kernel names.
 *
 *  \return clCreateProgramWithBuiltInKernels returns a valid non-zero program object and errcode_ret is set to
 * CL_SUCCESS if the program object is created successfully. Otherwise, it returns a NULL value with one of the
 * following error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if device_list is NULL or num_devices is zero.
 *  - CL_INVALID_VALUE if kernel_names is NULL or kernel_names contains a kernel name that is not supported by any of
 * the devices in device_list.
 *  - CL_INVALID_DEVICE if devices listed in device_list are not in the list of devices associated with context.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_program VC4CL_FUNC(clCreateProgramWithBuiltInKernels)(cl_context context, cl_uint num_devices,
    const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_program", clCreateProgramWithBuiltInKernels, "cl_context", context, "cl_uint", num_devices,
        "const cl_device_id*", device_list, "const char*", kernel_names, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)

    if(exceedsLimits<cl_uint>(num_devices, 1, 1) || device_list == nullptr)
        // only 1 device supported
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
            "Invalid number of devices given, a single is supported!");

    if(device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
        return returnError<cl_program>(
            CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__, "Device specified is not the VC4CL GPU device!");

    // no built-in kernels are supported!
    // TODO support FFT? From raspbery pi userland? libraries? FFT2?

    return returnError<cl_program>(
        CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "There are no supported built-in kernels");
}

/*!
 * OpenCL 1.2 specification, page 137:
 *
 *  Increments the program reference count. clCreateProgram does an implicit retain.
 *
 *  \return clRetainProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clRetainProgram)(cl_program program)
{
    VC4CL_PRINT_API_CALL("cl_int", clRetainProgram, "cl_program", program);
    CHECK_PROGRAM(toType<Program>(program))
    return toType<Program>(program)->retain();
}

/*!
 * OpenCL 1.2 specification, page 137:
 *
 *  Decrements the program reference count. The program object is deleted after all kernel objects associated with
 * program have been deleted and the program reference count becomes zero.
 *
 *  \return clReleaseProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clReleaseProgram)(cl_program program)
{
    VC4CL_PRINT_API_CALL("cl_int", clReleaseProgram, "cl_program", program);
    CHECK_PROGRAM(toType<Program>(program))
    return toType<Program>(program)->release();
}

/*!
 * OpenCL 1.2 specification, pages 138+:
 *
 *  Builds (compiles & links) a program executable from the program source or binary for all the devices or a specific
 * device(s) in the OpenCL context associated with program. OpenCL allows program executables to be built using the
 * source or the binary. clBuildProgram must be called for program created using either clCreateProgramWithSource or
 *  clCreateProgramWithBinary to build the program executable for one or more devices associated with program. If
 * program is created with clCreateProgramWithBinary, then the program binary must be an executable binary (not a
 * compiled binary or library). The executable binary can be queried using clGetProgramInfo(program,
 * CL_PROGRAM_BINARIES , ...) and can be specified to clCreateProgramWithBinary to create a new program object.
 *
 *  \param program is the program object.
 *
 *  \param device_list is a pointer to a list of devices associated with program. If device_list is a NULL value,
 *  the program executable is built for all devices associated with program for which a source or binary has been
 * loaded. If device_list is a non- NULL value, the program executable is built for devices specified in this list for
 * which a source or binary has been loaded.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param options is a pointer to a null-terminated string of characters that describes the build options to be used
 * for building the program executable. The list of supported options is described in section 5.6.4.
 *
 *  \param pfn_notify is a function pointer to a notification routine. The notification routine is a callback function
 * that an application can register and which will be called when the program executable has been built (successfully or
 * unsuccessfully). If pfn_notify is not NULL , clBuildProgram does not need to wait for the build to complete and can
 * return immediately once the build operation can begin. The build operation can begin if the context, program whose
 * sources are being compiled and linked, list of devices and build options specified are all valid and appropriate
 * host and device resources needed to perform the build are available. If pfn_notify is NULL , clBuildProgram does not
 * return until the build has completed. This callback function may be called asynchronously by the OpenCL
 * implementation. It is the application’s responsibility to ensure that the callback function is thread-safe.
 *
 *  \param user_data will be passed as an argument when pfn_notify is called. user_data can be NULL .
 *
 *  \return clBuildProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the
 * following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_INVALID_VALUE if device_list is NULL and num_devices is greater than zero, or if device_list is not NULL and
 * num_devices is zero.
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL .
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with program
 *  - CL_INVALID_BINARY if program is created with clCreateProgramWithBinary and devices listed in device_list do not
 * have a valid program binary loaded.
 *  - CL_INVALID_BUILD_OPTIONS if the build options specified by options are invalid.
 *  - CL_INVALID_OPERATION if the build of a program executable for any of the devices listed in device_list by a
 * previous call to clBuildProgram for program has not completed.
 *  - CL_COMPILER_NOT_AVAILABLE if program is created with clCreateProgramWithSource and a compiler is not available
 * i.e. CL_DEVICE_COMPILER_AVAILABLE specified in table 4.3 is set to CL_FALSE .
 *  - CL_BUILD_PROGRAM_FAILURE if there is a failure to build the program executable. This error will be returned if
 * clBuildProgram does not return until the build has completed.
 *  - CL_INVALID_OPERATION if there are kernel objects attached to program.
 *  - CL_INVALID_OPERATION if program was not created with clCreateProgramWithSource or clCreateProgramWithBinary.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clBuildProgram)(cl_program program, cl_uint num_devices, const cl_device_id* device_list,
    const char* options, void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data)
{
    VC4CL_PRINT_API_CALL("cl_int", clBuildProgram, "cl_program", program, "cl_uint", num_devices, "const cl_device_id*",
        device_list, "const char*", options, "void(CL_CALLBACK*)(cl_program program, void* user_data)", &pfn_notify,
        "void*", user_data);
    CHECK_PROGRAM(toType<Program>(program))
    if(num_devices > 1 || (num_devices == 0 && device_list != nullptr) || (num_devices > 0 && device_list == nullptr))
        // only 1 device supported
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Only the single VC4CL GPU device is supported!");
    if(device_list != nullptr && device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
        return returnError(CL_INVALID_DEVICE, __FILE__, __LINE__, "Invalid device given!");
    if(pfn_notify == nullptr && user_data != nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "User data was set, but callback wasn't!");

    std::string opts(options == nullptr ? "" : options);
    Program* p = toType<Program>(program);
    p->buildInfo.status = CL_BUILD_IN_PROGRESS;
    if(pfn_notify)
    {
        // "If pfn_notify is not NULL, clBuildProgram does not need to wait for the build to complete and can return
        // immediately once the build operation can begin"
        std::thread t{buildInner, object_wrapper<Program>{p}, opts, pfn_notify, user_data};
        t.detach();
        return CL_SUCCESS;
    }
    return buildInner(object_wrapper<Program>{p}, opts, pfn_notify, user_data);
}

/*!
 * OpenCL 1.2 specification, pages 140+:
 *
 *  Compiles a program’s source for all the devices or a specific device(s) in the OpenCL context associated with
 * program. The pre-processor runs before the program sources are compiled. The compiled binary is built for all devices
 * associated with program or the list of devices specified. The compiled binary can be queried using
 * clGetProgramInfo(program, CL_PROGRAM_BINARIES , ...) and can be specified to clCreateProgramWithBinary to create a
 * new program object.
 *
 *  \param program is the program object that is the compilation target.
 *
 *  \param device_list is a pointer to a list of devices associated with program. If device_list is a NULL value, the
 * compile is performed for all devices associated with program. If device_list is a non-NULL value, the compile is
 * performed for devices specified in this list.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param options is a pointer to a null-terminated string of characters that describes the compilation options to be
 * used for building the program executable. The list of supported options is as described in section 5.6.4.
 *
 *  \param num_input_headers specifies the number of programs that describe headers in the array referenced by
 * input_headers.
 *
 *  \param input_headers is an array of program embedded headers created with clCreateProgramWithSource.
 *
 *  \param header_include_names is an array that has a one to one correspondence with input_headers.
 *  Each entry in header_include_names specifies the include name used by source in program that comes from an embedded
 * header. The corresponding entry in input_headers identifies the program object which contains the header source to be
 * used. The embedded headers are first searched before the headers in the list of directories specified by the –I
 * compile option (as described in section 5.6.4.1). If multiple entries in header_include_names refer to the same
 * header name, the first one encountered will be used.
 *
 *  \param pfn_notify is a function pointer to a notification routine. The notification routine is a callback function
 * that an application can register and which will be called when the program executable has been built (successfully or
 * unsuccessfully). If pfn_notify is not NULL , clCompileProgram does not need to wait for the compiler to complete and
 * can return immediately once the compilation can begin. The compilation can begin if the context, program whose
 * sources are being compiled, list of devices, input headers, programs that describe input headers and compiler
 *  options specified are all valid and appropriate host and device resources needed to perform the compile are
 * available. If pfn_notify is NULL , clCompileProgram does not return until the compiler has completed. This callback
 * function may be called asynchronously by the OpenCL implementation. It is the application’s responsibility to ensure
 * that the callback function is thread-safe.
 *
 *  \param user_data will be passed as an argument when pfn_notify is called. user_data can be NULL .
 *
 *  \return clCompileProgram returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_PROGRAM if program is not a valid program object.
 *  - CL_INVALID_VALUE if device_list is NULL and num_devices is greater than zero, or if device_list is not NULL and
 * num_devices is zero.
 *  - CL_INVALID_VALUE if num_input_headers is zero and header_include_names or input_headers are not NULL or if
 * num_input_headers is not zero and header_include_names or input_headers are NULL .
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL .
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with program
 *  - CL_INVALID_COMPILER_OPTIONS if the compiler options specified by options are invalid.
 *  - CL_INVALID_OPERATION if the compilation or build of a program executable for any of the devices listed in
 * device_list by a previous call to clCompileProgram or clBuildProgram for program has not completed.
 *  - CL_COMPILER_NOT_AVAILABLE if a compiler is not available i.e. CL_DEVICE_COMPILER_AVAILABLE specified in table 4.3
 * is set to CL_FALSE .
 *  - CL_COMPILE_PROGRAM_FAILURE if there is a failure to compile the program source. This error will be returned if
 * clCompileProgram does not return until the compile has completed.
 *  - CL_INVALID_OPERATION if there are kernel objects attached to program.
 *  - CL_INVALID_OPERATION if program has no source i.e. it has not been created with clCreateProgramWithSource.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clCompileProgram)(cl_program program, cl_uint num_devices, const cl_device_id* device_list,
    const char* options, cl_uint num_input_headers, const cl_program* input_headers, const char** header_include_names,
    void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data)
{
    VC4CL_PRINT_API_CALL("cl_int", clCompileProgram, "cl_program", program, "cl_uint", num_devices,
        "const cl_device_id*", device_list, "const char*", options, "cl_uint", num_input_headers, "const cl_program*",
        input_headers, "const char**", header_include_names, "void(CL_CALLBACK*)(cl_program program, void* user_data)",
        &pfn_notify, "void*", user_data);
    CHECK_PROGRAM(toType<Program>(program))
    toType<Program>(program)->buildInfo.status = CL_BUILD_IN_PROGRESS;

    if(num_devices > 1 || (num_devices == 0 && device_list != nullptr) || (num_devices > 0 && device_list == nullptr))
        // only 1 device supported
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Only the single VC4CL GPU device is supported!");
    if(device_list != nullptr && device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
        return returnError(CL_INVALID_DEVICE, __FILE__, __LINE__, "Invalid device given!");

    if((num_input_headers == 0 && (header_include_names != nullptr || input_headers != nullptr)) ||
        (num_input_headers > 0 && (header_include_names == nullptr || input_headers == nullptr)))
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Invalid additional headers parameters!");

    if(pfn_notify == nullptr && user_data != nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "User data was set, but callback wasn't!");

    std::unordered_map<std::string, object_wrapper<Program>> embeddedHeaders;
    for(cl_uint i = 0; i < num_input_headers; ++i)
    {
        CHECK_PROGRAM(toType<Program>(input_headers[i]))
        if(toType<Program>(input_headers[i])->sourceCode.empty())
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
                buildString("Program for embedded header '%s' has no source code!", header_include_names[i]));
        //"If multiple entries in header_include_names refer to the same header name, the first one encountered will be
        // used."
        if(embeddedHeaders.find(header_include_names[i]) == embeddedHeaders.end())
            embeddedHeaders.emplace(
                std::string(header_include_names[i]), object_wrapper<Program>(toType<Program>(input_headers[i])));
    }

    const std::string opts(options == nullptr ? "" : options);
    if(pfn_notify)
    {
        // "If pfn_notify is not NULL, clCompileProgram does not need to wait for the compiler to complete and can
        // return immediately once the compilation can begin."
        std::thread t{compileInner, object_wrapper<Program>{toType<Program>(program)}, opts, embeddedHeaders,
            pfn_notify, user_data};
        t.detach();
        return CL_SUCCESS;
    }
    return compileInner(
        object_wrapper<Program>{toType<Program>(program)}, opts, std::move(embeddedHeaders), pfn_notify, user_data);
}

/*!
 * OpenCL 1.2 specification, pages 143+:
 *
 *  Links a set of compiled program objects and libraries for all the devices or a specific device(s) in the OpenCL
 * context and creates an executable. clLinkProgram creates a new program object which contains this executable. The
 * executable binary can be queried using clGetProgramInfo(program, CL_PROGRAM_BINARIES , ...) and can be specified to
 * clCreateProgramWithBinary to create a new program object. The devices associated with the returned program object
 * will be the list of devices specified by device_list or if device_list is NULL it will be the list of devices
 * associated with context.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param device_list is a pointer to a list of devices that are in context. If device_list is a NULL value, the link
 * is performed for all devices associated with context for which a compiled object is available. If device_list is a
 * non- NULL value, the link is performed for devices specified in this list for which a compiled object is available.
 *
 *  \param num_devices is the number of devices listed in device_list.
 *
 *  \param options is a pointer to a null-terminated string of characters that describes the link options to be used for
 * building the program executable. The list of supported options is as described in section 5.6.5.
 *
 *  \param num_input_programs specifies the number of programs in array referenced by input_programs. input_programs is
 * an array of program objects that are compiled binaries or libraries that are to be linked to create the program
 * executable. For each device in device_list or if device_list is NULL the list of devices associated with context,
 * the following cases occur:
 *   - All programs specified by input_programs contain a compiled binary or library for the device. In this case, a
 * link is performed to generate a program executable for this device.
 *   - None of the programs contain a compiled binary or library for that device. In this case, no link is performed and
 * there will be no program executable generated for this device.
 *   - All other cases will return a CL_INVALID_OPERATION error.
 *
 *  \param pfn_notify is a function pointer to a notification routine. The notification routine is a callback function
 * that an application can register and which will be called when the program executable has been built (successfully or
 * unsuccessfully). If pfn_notify is not NULL , clLinkProgram does not need to wait for the linker to complete and can
 * return immediately once the linking operation can begin. Once the linker has completed, the pfn_notify callback
 * function is called which returns the program object returned by clLinkProgram. The application can query the link
 * status and log for this program object. This callback function may be called asynchronously by the OpenCL
 * implementation. It is the application’s responsibility to ensure that the callback function is thread-safe. If
 * pfn_notify is NULL , clLinkProgram does not return until the linker has completed.
 *
 *  \param user_data will be passed as an argument when pfn_notify is called. user_data can be NULL .
 *
 *  The linking operation can begin if the context, list of devices, input programs and linker options specified are all
 * valid and appropriate host and device resources needed to perform the link are available. If the linking operation
 * can begin, clLinkProgram returns a valid non-zero program object. If pfn_notify is NULL , the errcode_ret will be
 * set to CL_SUCCESS if the link operation was successful and CL_LINK_FAILURE if there is a failure to link the
 * compiled binaries and/or libraries. If pfn_notify is not NULL , clLinkProgram does not have to wait until the linker
 * to complete and can return CL_SUCCESS in errcode_ret if the linking operation can begin. The pfn_notify callback
 * function will return a CL_SUCCESS or CL_LINK_FAILURE if the linking operation was successful or not.
 *
 *  \return Otherwise clLinkProgram returns a NULL program object with an appropriate error in errcode_ret.
 *  The application should query the linker status of this program object to check if the link was successful or not.
 * The list of errors that can be returned are:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if device_list is NULL and num_devices is greater than zero, or if device_list is not NULL and
 * num_devices is zero.
 *  - CL_INVALID_VALUE if num_input_programs is zero and input_programs is NULL or if num_input_programs is zero and
 * input_programs is not NULL or if num_input_programs is not zero and input_programs is NULL .
 *  - CL_INVALID_PROGRAM if programs specified in input_programs are not valid program objects.
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL .
 *  - CL_INVALID_DEVICE if OpenCL devices listed in device_list are not in the list of devices associated with context
 *  - CL_INVALID_LINKER_OPTIONS if the linker options specified by options are invalid.
 *  - CL_INVALID_OPERATION if the compilation or build of a program executable for any of the devices listed in
 * device_list by a previous call to clCompileProgram or clBuildProgram for program has not completed.
 *  - CL_INVALID_OPERATION if the rules for devices containing compiled binaries or libraries as described in
 * input_programs argument above are not followed.
 *  - CL_LINKER_NOT_AVAILABLE if a linker is not available i.e. CL_DEVICE_LINKER_AVAILABLE specified in table 4.3 is set
 * to CL_FALSE .
 *  - CL_LINK_PROGRAM_FAILURE if there is a failure to link the compiled binaries and/or libraries.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_program VC4CL_FUNC(clLinkProgram)(cl_context context, cl_uint num_devices, const cl_device_id* device_list,
    const char* options, cl_uint num_input_programs, const cl_program* input_programs,
    void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_program", clLinkProgram, "cl_context", context, "cl_uint", num_devices,
        "const cl_device_id*", device_list, "const char*", options, "cl_uint", num_input_programs, "const cl_program*",
        input_programs, "void(CL_CALLBACK*)(cl_program program, void* user_data)", &pfn_notify, "void*", user_data,
        "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_program)
    if((num_devices == 0) != (device_list == nullptr))
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "No devices given!");
    if(num_devices > 1 ||
        (device_list != nullptr && device_list[0] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase()))
        return returnError<cl_program>(CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__,
            "Invalid device(s) given, only the VC4CL GPU device is supported!");
    if(num_input_programs == 0 || input_programs == nullptr)
        return returnError<cl_program>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Invalid input program");

    std::vector<object_wrapper<Program>> inputPrograms;
    inputPrograms.reserve(num_input_programs);
    for(cl_uint i = 0; i < num_input_programs; ++i)
    {
        CHECK_PROGRAM_ERROR_CODE(toType<Program>(input_programs[i]), errcode_ret, cl_program)
        inputPrograms.emplace_back(toType<Program>(input_programs[i]));
    }

    // create a new empty program the result of the linking is inserted into
    const std::string opts(options == nullptr ? "" : options);
    auto type =
        opts.find("-create-library") == std::string::npos ? CreationType::INTERMEDIATE_LANGUAGE : CreationType::LIBRARY;
    Program* newProgram = newOpenCLObject<Program>(toType<Context>(context), std::vector<char>{}, type);
    CHECK_ALLOCATION_ERROR_CODE(newProgram, errcode_ret, cl_program)

    newProgram->buildInfo.status = CL_BUILD_IN_PROGRESS;
    if(pfn_notify)
    {
        // "If pfn_notify is not NULL, clLinkProgram does not need to wait for the linker to complete and can return
        // immediately once the linking operation can begin."
        std::thread t{linkInner, object_wrapper<Program>(newProgram), opts, inputPrograms, pfn_notify, user_data};
        t.detach();
    }
    else
    {
        cl_int status = linkInner(object_wrapper<Program>(newProgram), opts, inputPrograms, pfn_notify, user_data);
        if(status != CL_SUCCESS)
        {
            // on linker error, we need to discard the newly created program, otherwise it will be leaked
            ignoreReturnValue(newProgram->release(), __FILE__, __LINE__, "This should never fail!");
            return returnError<cl_program>(status, errcode_ret, __FILE__, __LINE__, "Linking failed!");
        }
    }
    RETURN_OBJECT(newProgram->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, page 150:
 *
 *  Allows the implementation to release the resources allocated by the OpenCL compiler for platform.
 *  This is a hint from the application and does not guarantee that the compiler will not be used in the future or that
 * the compiler will actually be unloaded by the implementation. Calls to clBuildProgram, clCompileProgram or
 * clLinkProgram after clUnloadPlatformCompiler will reload the compiler, if necessary, to build the appropriate
 * program executable.
 *
 *  \return clUnloadPlatformCompiler returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns
 * one of the following errors:
 *  - CL_INVALID_PLATFORM if platform is not a valid platform.
 */
cl_int VC4CL_FUNC(clUnloadPlatformCompiler)(cl_platform_id platform)
{
    VC4CL_PRINT_API_CALL("cl_int", clUnloadPlatformCompiler, "cl_platform_id", platform);
    // does nothing
    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 151+:
 *
 *  Returns information about the program object.
 *
 *  \param program specifies the program object being queried.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information
 * returned in param_value by clGetProgramInfo is described in table 5.13.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is
 * NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.13.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret
 * is NULL , it is ignored.
 *
 *  \return clGetProgramInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return
 * type as described in table 5.13 and param_value is not NULL .
 *  - CL_INVALID_PROGRAM if program is a not a valid program object.
 *  - CL_INVALID_PROGRAM_EXECUTABLE if param_name is CL_PROGRAM_NUM_KERNELS or CL_PROGRAM_KERNEL_NAMES and a successful
 * program executable has not been built for at least one device in the list of devices associated with program.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clUnloadCompiler)(void)
{
    VC4CL_PRINT_API_CALL("cl_int", clUnloadCompiler, "void", "");
    // does nothing
    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 154+:
 *
 *  Returns build information for each device in the program object.
 *
 *  \param program specifies the program object being queried.
 *
 *  \param device specifies the device for which build information is being queried. device must be a valid device
 * associated with program.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information
 * returned in param_value by clGetProgramBuildInfo is described in table 5.14.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is
 * NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.14.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret
 * is NULL , it is ignored.
 *
 *  \return clGetProgramBuildInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one
 * of the following errors:
 *  - CL_INVALID_DEVICE if device is not in the list of devices associated with program.
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return
 * type as described in table 5.14 and param_value is not NULL .
 *  - CL_INVALID_PROGRAM if program is a not a valid program object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetProgramInfo)(cl_program program, cl_program_info param_name, size_t param_value_size,
    void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetProgramInfo, "cl_program", program, "cl_program_info", param_name, "size_t",
        param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_PROGRAM(toType<Program>(program))
    return toType<Program>(program)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int VC4CL_FUNC(clGetProgramBuildInfo)(cl_program program, cl_device_id device, cl_program_build_info param_name,
    size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetProgramBuildInfo, "cl_program", program, "cl_device_id", device,
        "cl_program_build_info", param_name, "size_t", param_value_size, "void*", param_value, "size_t*",
        param_value_size_ret);
    CHECK_PROGRAM(toType<Program>(program))
    CHECK_DEVICE(toType<Device>(device))
    return toType<Program>(program)->getBuildInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
