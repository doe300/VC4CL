/*
 * Header containing the binary format layout shared between the VC4C compiler and the VC4CL runtime.
 *
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_SHARED_BINARY_HEADER_H
#define VC4CL_SHARED_BINARY_HEADER_H

#include "../Bitfield.h"

#include <array>
#include <string>
#include <vector>

#ifdef VC4CL_BITFIELD
namespace vc4cl
#else
namespace vc4c
#endif
{
    enum class AddressSpace : unsigned char;

    /*
     * Binary layout (64-bit rows, item lengths not to scale):
     *
     * | type size | vector elements | name length | type name length | decorations | type flags |
     * | name ...
     *   ...                                                                                     |
     * | type name ...
     *   ...                                                                                     |
     */
    struct ParamHeader : public Bitfield<uint64_t>
    {
        explicit ParamHeader(uint64_t val = 0) noexcept : Bitfield(val) {}

        /*
         * The size of the parameter in bytes
         *
         * Type considerations: The maximum vector type-size is int16 (long16), which has 64(128) bytes, minimum a
         * size of 1 byte (char). 13 bits fit also structure parameters up to 8 KB.
         */
        BITFIELD_ENTRY(Size, uint16_t, 0, Tredecuple)
        /*
         * The number of vector-elements for the parameter
         *
         * Type considerations: 1 to 16 elements are supported, fits in 5 bits
         */
        BITFIELD_ENTRY(VectorElements, uint8_t, 13, Quintuple)
        /*
         * The length of the parameter name in characters
         *
         * Type considerations: byte can contain 255 characters, ushort 64k, 2^12 4096
         */
        BITFIELD_ENTRY(NameLength, uint16_t, 18, Duodecuple)
        /*
         * The length of the parameter type-name in characters
         *
         * Type considerations: byte can contain 255 characters, ushort 64k, 2^12 4096
         */
        BITFIELD_ENTRY(TypeNameLength, uint16_t, 30, Duodecuple)
        /*
         * The grouping of all parameter decorations
         */
        BITFIELD_ENTRY(Decorations, uint16_t, 42, Decuple)
        /**
         * Whether this parameter is being read, only valid for pointers and images
         */
        BITFIELD_ENTRY(Input, bool, 42, Bit)
        /**
         * Whether this parameter is being written, only valid for pointers and images
         */
        BITFIELD_ENTRY(Output, bool, 43, Bit)
        /**
         * Whether this parameter is zero-extended
         */
        BITFIELD_ENTRY(ZeroExtend, bool, 44, Bit)
        /**
         * Whether this parameter is sign-extended
         */
        BITFIELD_ENTRY(SignExtend, bool, 45, Bit)
        /**
         * Whether this parameter is constant, only valid for pointers
         */
        BITFIELD_ENTRY(Constant, bool, 46, Bit)
        /**
         * Whether the memory behind this parameter is guaranteed to not be aligned (overlap with other memory areas),
         * only valid for pointers
         */
        BITFIELD_ENTRY(Restricted, bool, 47, Bit)
        /**
         * Whether the memory behind this parameter is volatile, only valid for pointers
         */
        BITFIELD_ENTRY(Volatile, bool, 48, Bit)
        /**
         * Whether the "pointer" parameter will be passed by value to clSetKernelArg
         */
        BITFIELD_ENTRY(ByValue, bool, 49, Bit)

        //// 2 Bits unused

        /*
         * Whether this parameter lowered into shared VPM memory and therefore no temporary buffers needs to
         * be allocated for it.
         *
         * NOTE: This is only valid for __local parameters where the VC4C compiler can deduce the maximum accessed
         * range.
         */
        BITFIELD_ENTRY(Lowered, bool, 54, Bit)

        /*
         * The address space for this parameter
         *
         * Type considerations: There are 5 address-spaces, fit into 4 bits
         */
        BITFIELD_ENTRY(AddressSpace, AddressSpace, 55, Quadruple)
        /*
         * Whether this parameter is an image
         */
        BITFIELD_ENTRY(Image, bool, 59, Bit)
        /*
         * Whether the parameter is a pointer-type
         */
        BITFIELD_ENTRY(Pointer, bool, 60, Bit)
        /*
         * Whether the parameter is a floating-point type
         */
        BITFIELD_ENTRY(FloatingType, bool, 61, Bit)
        /*
         * Whether this parameter is known to be a signed integer type
         */
        BITFIELD_ENTRY(Signed, bool, 62, Bit)
        /*
         * Whether this parameter is known to be an unsigned integer type
         */
        BITFIELD_ENTRY(Unsigned, bool, 63, Bit)

        /**
         * The parameter name in the source code
         */
        std::string name;
        /**
         * The parameter type name in the source code
         */
        std::string typeName;

        inline void setName(const std::string& name)
        {
            this->name = name;
            setNameLength(static_cast<uint16_t>(name.size()));
        }

        inline void setTypeName(const std::string& name)
        {
            typeName = name;
            setTypeNameLength(static_cast<uint16_t>(name.size()));
        }

        inline bool isReadOnly() const
        {
            return getInput() && !getOutput();
        }

        inline bool isWriteOnly() const
        {
            return getOutput() && !getInput();
        }

        std::string to_string() const;
        void toBinaryData(std::vector<uint64_t>& data) const;
        static ParamHeader fromBinaryData(const std::vector<uint64_t>& data, std::size_t& dataIndex);
    };

    /**
     * Contains information about the implicit UNIFORMs (work-group info, etc.) actually used in the kernel
     */
    struct KernelUniforms : public Bitfield<uint32_t>
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

        size_t countUniforms() const;
    };

    /*
     * Binary layout (64-bit rows, item lengths not to scale):
     *
     * | offset | length | name length | parameter count |
     * | compilation work-group size | item merge-factor |
     * | uniforms used        | << unused >>             |
     * | name ...
     *   ...                                             |
     * | parameters ...
     *   ...                                             |
     */
    struct KernelHeader : public Bitfield<uint64_t>
    {
        explicit KernelHeader(const std::size_t& numParameters);

        /*
         * The offset in multiple of 64-bit from the start of the module
         *
         * Type considerations: ushort supports a maximum kernel offset (of the last kernel) of 512 kB (64k * 8
         * byte), 24-bit integer up to 128 MB
         */
        BITFIELD_ENTRY(Offset, size_t, 0, Quattuorvigintuple)
        /*
         * The number of 64-bit instructions
         *
         * Type considerations: ushort supports a maximum kernel size of 64k instructions (512 kB), 22-bit integer
         * up to 32 MB
         */
        BITFIELD_ENTRY(Length, size_t, 24, Duovigintuple)
        /*
         * The length of the kernel-name (in bytes) excluding padding bytes. NOTE: Do not set this value manually!
         *
         * Type considerations: byte can contain 255 characters, decuple up to 1023, ushort 64k
         */
        BITFIELD_ENTRY(NameLength, size_t, 46, Decuple)
        /*
         * The number of parameters. NOTE: Do not set this value manually!
         *
         * Type considerations: Since we read all parameters at the start, only ~70 parameters (number of registers)
         * are supported
         */
        BITFIELD_ENTRY(ParamCount, size_t, 56, Byte)
        /*
         * The 3 dimensions for the work-group size specified in the source code
         */
        std::array<uint16_t, 3> workGroupSize;
        /*
         * The merge factor for merged work-items (the number of work-items executed by a single QPU)
         */
        uint8_t workItemMergeFactor;
        /*
         * Bit-field determining the implicit UNIFORMs used by this kernel. Depending on this field, the
         * UNIFORM-values are created host-side
         */
        KernelUniforms uniformsUsed;

        /**
         * The kernel name in the source code
         */
        std::string name;
        /**
         * The explicit parameters, as in the source code
         */
        std::vector<ParamHeader> parameters;

        inline void setName(const std::string& name)
        {
            this->name = name;
            setNameLength(name.size());
        }

        inline void addParameter(const ParamHeader& param)
        {
            parameters.push_back(param);
            setParamCount(parameters.size());
        }

        size_t getExplicitUniformCount() const;

        std::string to_string() const;
        void toBinaryData(std::vector<uint64_t>& data) const;
        static KernelHeader fromBinaryData(const std::vector<uint64_t>& data, std::size_t& dataIndex);
    };

    /*
     * Binary layout (64-bit rows, item lengths not to scale):
     *
     * | MAGIC NUMBER                     | MAGIC NUMBER                        |
     * | num kernels | global-data offset | global-data size | stack-frame size |
     */
    struct ModuleHeader : public Bitfield<uint64_t>
    {
        explicit ModuleHeader(uint64_t val = 0) noexcept : Bitfield(val) {}

        /*
         * Magic number to identify QPU assembler code (machine code)
         */
        static const uint32_t QPUASM_MAGIC_NUMBER;

        /*
         * The number of kernels in this module. NOTE: Do not set this number manually!
         *
         * Type considerations: byte supports 255 kernels, decuple 1024 and ushort up to 64k
         */
        BITFIELD_ENTRY(KernelCount, size_t, 0, Decuple)
        /*
         * The offset of global-data in multiples of 64-bit from the start of the compilation unit
         *
         * Type considerations: ushort supports an offset of 512 kB, vigintuple (20 bits) up to 8 MB and 24-bit
         * integer up to 128 MB
         */
        BITFIELD_ENTRY(GlobalDataOffset, size_t, 10, Short)
        /*
         * The size of the global data segment in multiples of 64-bit
         *
         * Type considerations: ushort supports 512 kB of global data, vigintuple (20 bits) up to 8 MB and 24-bit
         * integer up to 128 MB
         */
        BITFIELD_ENTRY(GlobalDataSize, size_t, 26, Vigintuple)
        /*
         * The size of a single stack-frame, appended to the global-data segment. In multiples of 64-bit
         *
         * Type considerations: ushort supports 512 kB of stack-frame, vigintuple (20 bits) up to 8 MB and 24-bit
         * integer up to 128 MB
         */
        BITFIELD_ENTRY(StackFrameSize, size_t, 46, Short)

        /**
         * The metadata for all kernels contained in this module
         */
        std::vector<KernelHeader> kernels;

        inline void addKernel(const KernelHeader& kernel)
        {
            kernels.push_back(kernel);
            setKernelCount(kernels.size());
        }

        std::string to_string() const;
        std::vector<uint64_t> toBinaryData(const std::vector<uint64_t>& globalData, uint16_t numStackWords);
        static ModuleHeader fromBinaryData(const std::vector<uint64_t>& data);
    };

} // namespace vc4c

#endif /* VC4CL_SHARED_BINARY_HEADER_H */
