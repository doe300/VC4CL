/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_PROGRAM
#define VC4CL_PROGRAM

#include "Bitfield.h"
#include "Context.h"

#include <bitset>
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
    enum class AddressSpace
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

    /*
     * NOTE: ParamInfo KernelInfo and ModuleInfo need to map exactly to the corresponding types in the VC4C project!
     */
    struct KernelUniforms : public Bitfield<uint64_t>
    {
        BITFIELD_ENTRY(WorkDimensionsUsed, bool, 0, Bit)
        BITFIELD_ENTRY(LocalSizesUsed, bool, 1, Bit)
        BITFIELD_ENTRY(LocalIDsUsed, bool, 2, Bit)
        BITFIELD_ENTRY(NumGroupsXUsed, bool, 3, Bit)
        BITFIELD_ENTRY(NumGroupsYUsed, bool, 4, Bit)
        BITFIELD_ENTRY(NumGroupsZUsed, bool, 5, Bit)
        BITFIELD_ENTRY(GroupIDXUsed, bool, 6, Bit)
        BITFIELD_ENTRY(GroupIDYUsed, bool, 7, Bit)
        BITFIELD_ENTRY(GroupIDZUsed, bool, 8, Bit)
        BITFIELD_ENTRY(GlobalOffsetXUsed, bool, 9, Bit)
        BITFIELD_ENTRY(GlobalOffsetYUsed, bool, 10, Bit)
        BITFIELD_ENTRY(GlobalOffsetZUsed, bool, 11, Bit)
        BITFIELD_ENTRY(GlobalDataAddressUsed, bool, 12, Bit)
        BITFIELD_ENTRY(UniformAddressUsed, bool, 13, Bit)
        BITFIELD_ENTRY(MaxGroupIDXUsed, bool, 14, Bit)
        BITFIELD_ENTRY(MaxGroupIDYUsed, bool, 15, Bit)
        BITFIELD_ENTRY(MaxGroupIDZUsed, bool, 16, Bit)

        inline size_t countUniforms() const
        {
            std::bitset<64> tmp(value);
            return tmp.count();
        }
    };

    /*
     * NOTE: ParamInfo KernelInfo and ModuleInfo need to map exactly to the corresponding types in the VC4C project!
     */
    struct ParamInfo : private Bitfield<uint64_t>
    {
        explicit ParamInfo(uint64_t val = 0) noexcept : Bitfield(val) {}

        // the size of this parameter in bytes (e.g. 4 for pointers)
        BITFIELD_ENTRY(Size, uint16_t, 0, Tredecuple)
        // the number of components for vector-parameters
        BITFIELD_ENTRY(VectorElements, uint8_t, 13, Quintuple)
        BITFIELD_ENTRY(NameLength, uint16_t, 18, Duodecuple)
        BITFIELD_ENTRY(TypeNameLength, uint16_t, 30, Duodecuple)
        // whether this parameter is being read, only valid for pointers and images
        BITFIELD_ENTRY(Input, bool, 42, Bit)
        // whether this parameter is being written, only valid for pointers and images
        BITFIELD_ENTRY(Output, bool, 43, Bit)

        //// ZERO_EXTEND and SIGN_EXTEND are not used

        // whether this parameter is constant, only valid for pointers
        BITFIELD_ENTRY(Constant, bool, 46, Bit)
        // whether the memory behind this parameter is guaranteed to not be aligned (overlap with other memory areas),
        // only valid for pointers
        BITFIELD_ENTRY(Restricted, bool, 47, Bit)
        // whether the memory behind this parameter is volatile, only valid for pointers
        BITFIELD_ENTRY(Volatile, bool, 48, Bit)
        // whether the "pointer" parameter will be passed by value to clSetKernelArg
        BITFIELD_ENTRY(ByValue, bool, 49, Bit)

        //// 2 Bits of decoration currently unused
        //// 2 Bits unused

        // whether the parameter is lowered (e.g. into VPM) and does not need any (temporary) buffer allocated for it
        BITFIELD_ENTRY(Lowered, bool, 54, Bit)
        // the parameter's address space, only valid for pointers
        // OpenCL default address space is "private"
        BITFIELD_ENTRY(AddressSpace, AddressSpace, 55, Quadruple)
        // whether this parameter is an image
        BITFIELD_ENTRY(Image, bool, 59, Bit)
        // whether this parameter is a pointer to data
        BITFIELD_ENTRY(Pointer, bool, 60, Bit)
        // whether the parameter type is a floating-point type (e.g. float, half, double)
        BITFIELD_ENTRY(FloatingType, bool, 61, Bit)
        // whether this parameter is known to be signed
        BITFIELD_ENTRY(Signed, bool, 62, Bit)
        // whether this parameter is known to be unsigned
        BITFIELD_ENTRY(Unsigned, bool, 63, Bit)

        // the parameter name
        std::string name;
        // the parameter type-name, e.g. "<16 x i32>*"
        std::string type;

        inline bool isReadOnly() const
        {
            return getInput() && !getOutput();
        }

        inline bool isWriteOnly() const
        {
            return getOutput() && !getInput();
        }
    };

    /*
     * NOTE: ParamInfo KernelInfo and ModuleInfo need to map exactly to the corresponding types in the VC4C project!
     */
    struct KernelInfo : private Bitfield<uint64_t>
    {
        explicit KernelInfo(uint64_t val = 0) noexcept : Bitfield(val) {}

        // the offset of the instruction belonging to the kernel, in instructions (8 byte)
        BITFIELD_ENTRY(Offset, uint32_t, 0, Quattuorvigintuple)
        // the number of 64-bit instructions in the kernel
        BITFIELD_ENTRY(Length, uint32_t, 24, Duovigintuple)
        BITFIELD_ENTRY(NameLength, uint16_t, 46, Decuple)
        BITFIELD_ENTRY(ParamCount, uint8_t, 56, Byte)

        // the kernel-name
        std::string name;
        // the work-group size specified at compile-time
        std::array<std::size_t, kernel_config::NUM_DIMENSIONS> compileGroupSizes;
        // bit-field determining the implicit UNIFORMs used by this kernel. Depending on this field, the UNIFORM-values
        // are created host-side
        KernelUniforms uniformsUsed;
        // the info for all explicit parameters
        std::vector<ParamInfo> params;

        size_t getExplicitUniformCount() const __attribute__((pure));
    };

    /*
     * NOTE: ParamInfo KernelInfo and ModuleInfo need to map exactly to the corresponding types in the VC4C project!
     */
    struct ModuleInfo : Bitfield<uint64_t>
    {
        explicit ModuleInfo(uint64_t val = 0) noexcept : Bitfield(val) {}

        // number of kernel-infos in this module
        BITFIELD_ENTRY(InfoCount, uint16_t, 0, Decuple)
        // offset of global-data in multiples of 64-bit
        BITFIELD_ENTRY(GlobalDataOffset, uint16_t, 10, Short)
        // size of the global data segment in multiples of 64-bit
        BITFIELD_ENTRY(GlobalDataSize, uint32_t, 26, Vigintuple)
        // size of a single stack-frame, appended to the global-data segment. In multiples of 64-bit
        BITFIELD_ENTRY(StackFrameSize, uint16_t, 46, Short)

        std::vector<KernelInfo> kernelInfos;
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
        ModuleInfo moduleInfo;

        BuildStatus getBuildStatus() const __attribute__((pure));

    private:
        cl_int extractModuleInfo();
        cl_int extractKernelInfo(cl_ulong** ptr);
    };

} /* namespace vc4cl */

#endif /* VC4CL_PROGRAM */
