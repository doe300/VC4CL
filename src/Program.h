/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_PROGRAM
#define VC4CL_PROGRAM

#include "Bitfield.h"
#include "Context.h"
#include "shared/BinaryHeader.h"

#include <unordered_map>
#include <vector>

namespace vc4cl
{
    enum class BuildStatus
    {
        // not yet built at all
        NOT_BUILD = 0,
        // compiled but not yet linked
        COMPILED = 1,
        // linked and compiled -> done
        DONE = 2
    };

    struct BuildInfo
    {
        cl_build_status status = CL_BUILD_NONE;
        std::string options;
        std::string log;
    };

    // THIS NEEDS TO HAVE THE SAME VALUES AS THE AddressSpace TYPE IN THE COMPILER!!
    enum class AddressSpace : uint8_t
    {
        GENERIC = 0,
        PRIVATE = 1,
        GLOBAL = 2,
        CONSTANT = 3,
        LOCAL = 4
    };

    enum class CreationType
    {
        // program was created from OpenCL C source-code
        SOURCE,
        // program was created from SPIR/SPIR-V IL
        INTERMEDIATE_LANGUAGE,
        // fake a completely compiled library. This behaves mostly like INTERMEDIATE_LANGUAGE, but reports the library
        // binary type
        LIBRARY,
        // program was created from pre-compiled binary
        BINARY
    };

    using ProgramReleaseCallback = void(CL_CALLBACK*)(cl_program program, void* user_data);
    struct SPIRVSpecializationConstant
    {
        uint32_t constantId;
        std::vector<uint8_t> data;
    };

    class Program final : public Object<_cl_program, CL_INVALID_PROGRAM>, public HasContext
    {
    public:
        Program(Context* context, const std::vector<char>& code, CreationType type);
        ~Program() noexcept override;

        /*
         * Compiles the program from OpenCL C source to intermediate representation (SPIR/LLVM IR, SPIR-V)
         */
        CHECK_RETURN cl_int compile(const std::string& options,
            const std::unordered_map<std::string, object_wrapper<Program>>& embeddedHeaders);
        /*
         * Links the intermediate representation (if supported) and compiles to machine code
         */
        CHECK_RETURN cl_int link(const std::string& options, const std::vector<object_wrapper<Program>>& programs = {});
        CHECK_RETURN cl_int getInfo(
            cl_program_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
        CHECK_RETURN cl_int getBuildInfo(
            cl_program_build_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

        // the program's source, OpenCL C-code or LLVM IR / SPIR-V
        std::vector<char> sourceCode;
        /*
         * the program's intermediate code (e.g. SPIR or SPIR-V)
         * This code is pre-compiled, but not yet linked or compiled to QPU code
         *
         * We choose single bytes, since we do not know the unit size of LLVM IR (SPIR-V has words of 32 bit)
         */
        std::vector<uint8_t> intermediateCode;
        // the machine-code, VC4C binary
        std::vector<uint64_t> binaryCode;
        // the global-data segment
        std::vector<uint64_t> globalData;
        // the way the program was created
        CreationType creationType;

        BuildInfo buildInfo;

        // the module-info, extracted from the VC4C binary
        // if this is set, the program is completely finished compiling
        ModuleHeader moduleInfo;

        BuildStatus getBuildStatus() const __attribute__((pure));

        CHECK_RETURN cl_int setReleaseCallback(ProgramReleaseCallback callback, void* userData);

        CHECK_RETURN cl_int setSpecializationConstant(cl_uint id, std::size_t numBytes, const void* data);

    private:
        cl_int extractModuleInfo();

        std::vector<std::pair<ProgramReleaseCallback, void*>> callbacks;
        std::vector<SPIRVSpecializationConstant> specializations;
    };

} /* namespace vc4cl */

#endif /* VC4CL_PROGRAM */
