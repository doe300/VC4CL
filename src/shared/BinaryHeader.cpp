/*
 * Implementation for binary representation of module header data shared across VC4C compiler and VC4CL run-time.
 *
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "./BinaryHeader.h"

#include <algorithm>
#include <bitset>

#ifdef VC4CL_BITFIELD
#include "../Program.h"
using namespace vc4cl;
#else
#include "../Locals.h"
using namespace vc4c;
#define HAS_DECORATIONS 1
#endif

#ifndef LCOV_EXCL_START
#define LCOV_EXCL_START /* LCOV_EXCL_START */
#endif
#ifndef LCOV_EXCL_STOP
#define LCOV_EXCL_STOP /* LCOV_EXCL_STOP */
#endif

template <typename T, typename VT = std::vector<T>>
static std::string toString(const VT& values, const std::string& separator = ", ")
{
    std::string tmp;
    for(const T& val : values)
    {
        tmp.append(val.to_string()).append(separator);
    }
    return tmp.substr(0, tmp.size() - separator.size());
}

template <>
std::string toString<std::string>(const std::vector<std::string>& values, const std::string& separator)
{
    std::string tmp;
    for(const std::string& val : values)
    {
        tmp.append(val).append(separator);
    }
    return tmp.substr(0, tmp.size() - separator.size());
}

static void writeString(const std::string& text, std::vector<uint64_t>& data)
{
    auto numWords = text.size() / sizeof(uint64_t);
    numWords += (text.size() % sizeof(uint64_t) != 0) ? 1 : 0;
    auto currentIndex = data.size();
    data.resize(data.size() + numWords);
    std::copy(text.begin(), text.end(), reinterpret_cast<char*>(&data[currentIndex]));
}

static std::string readString(const std::vector<uint64_t>& data, std::size_t& dataIndex, std::size_t numBytes)
{
    std::string result(reinterpret_cast<const char*>(&data[dataIndex]), numBytes);
    dataIndex += numBytes / sizeof(uint64_t);
    dataIndex += (numBytes % sizeof(uint64_t) != 0) ? 1 : 0;
    return result;
}

LCOV_EXCL_START
std::string ParamHeader::to_string() const
{
    // address space
    return std::string((getPointer() && getAddressSpace() == AddressSpace::CONSTANT) ? "__constant " : "") +
        std::string((getPointer() && getAddressSpace() == AddressSpace::GLOBAL) ? "__global " : "") +
        std::string((getPointer() && getAddressSpace() == AddressSpace::LOCAL) ? "__local " : "") +
        std::string((getPointer() && getAddressSpace() == AddressSpace::PRIVATE) ? "__private " : "") +
#ifdef HAS_DECORATIONS
        // access qualifier
        (static_cast<ParameterDecorations>(getDecorations()) != ParameterDecorations::NONE ?
                toString(static_cast<ParameterDecorations>(getDecorations())) + " " :
                "") +
#endif
        // type + name
        ((typeName) + " ") + (name + " (") + (std::to_string(getSize()) + " B, ") +
        std::to_string(getVectorElements()) + " items)" + (getLowered() ? " (lowered)" : "");
}
LCOV_EXCL_STOP

void ParamHeader::toBinaryData(std::vector<uint64_t>& data) const
{
    data.push_back(value);
    writeString(name, data);
    writeString(typeName, data);
}

ParamHeader ParamHeader::fromBinaryData(const std::vector<uint64_t>& data, std::size_t& dataIndex)
{
    ParamHeader param{data[dataIndex]};
    ++dataIndex;
    param.name = readString(data, dataIndex, param.getNameLength());
    param.typeName = readString(data, dataIndex, param.getTypeNameLength());
    return param;
}

size_t KernelUniforms::countUniforms() const
{
    std::bitset<sizeof(value) * 8> tmp(value);
    return tmp.count();
}

KernelHeader::KernelHeader(const std::size_t& numParameters) : Bitfield(0), workGroupSize(), workItemMergeFactor(0)
{
    parameters.reserve(numParameters);
    workGroupSize.fill(0);
}

size_t KernelHeader::getExplicitUniformCount() const
{
    size_t count = 0;
    for(const auto& info : parameters)
        count += info.getVectorElements();
    return count;
}

LCOV_EXCL_START
std::string KernelHeader::to_string() const
{
    std::vector<std::string> uniformsSet;
    if(uniformsUsed.getWorkDimensionsUsed())
        uniformsSet.emplace_back("dims");
    if(uniformsUsed.getLocalSizesUsed())
        uniformsSet.emplace_back("lSize");
    if(uniformsUsed.getLocalIDsUsed())
        uniformsSet.emplace_back("lids");
    if(uniformsUsed.getNumGroupsXUsed())
        uniformsSet.emplace_back("numX");
    if(uniformsUsed.getNumGroupsYUsed())
        uniformsSet.emplace_back("numY");
    if(uniformsUsed.getNumGroupsZUsed())
        uniformsSet.emplace_back("numZ");
    if(uniformsUsed.getGroupIDXUsed())
        uniformsSet.emplace_back("gidX");
    if(uniformsUsed.getGroupIDYUsed())
        uniformsSet.emplace_back("gidY");
    if(uniformsUsed.getGroupIDZUsed())
        uniformsSet.emplace_back("gidZ");
    if(uniformsUsed.getGlobalOffsetXUsed())
        uniformsSet.emplace_back("offX");
    if(uniformsUsed.getGlobalOffsetYUsed())
        uniformsSet.emplace_back("offY");
    if(uniformsUsed.getGlobalOffsetZUsed())
        uniformsSet.emplace_back("offZ");
    if(uniformsUsed.getGlobalDataAddressUsed())
        uniformsSet.emplace_back("global");
    if(uniformsUsed.getUniformAddressUsed())
        uniformsSet.emplace_back("unifAddr");
    if(uniformsUsed.getMaxGroupIDXUsed())
        uniformsSet.emplace_back("maxGidX");
    if(uniformsUsed.getMaxGroupIDYUsed())
        uniformsSet.emplace_back("maxGidY");
    if(uniformsUsed.getMaxGroupIDZUsed())
        uniformsSet.emplace_back("maxGidZ");
    const std::string uniformsString =
        uniformsSet.empty() ? "" : (std::string(" (") + ::toString<std::string>(uniformsSet) + ")");

    auto mergeFactor =
        workItemMergeFactor ? (" (work-item merge factor: " + std::to_string(workItemMergeFactor) + ")") : "";

    return std::string("Kernel '") + (name + "' with ") + (std::to_string(getLength()) + " instructions, offset ") +
        (std::to_string(getOffset()) + ", with following parameters: ") + ::toString<ParamHeader>(parameters) +
        uniformsString + mergeFactor;
}
LCOV_EXCL_STOP

void KernelHeader::toBinaryData(std::vector<uint64_t>& data) const
{
    data.push_back(value);
    auto secondWord = uint64_t{workGroupSize[0]} | (uint64_t{workGroupSize[1]} << uint64_t{16}) |
        (uint64_t{workGroupSize[2]} << uint64_t{32}) | (uint64_t{workItemMergeFactor} << uint64_t{48});
    data.push_back(secondWord);
    data.push_back(uniformsUsed.value);
    writeString(name, data);
    for(const auto& param : parameters)
        param.toBinaryData(data);
}

KernelHeader KernelHeader::fromBinaryData(const std::vector<uint64_t>& data, std::size_t& dataIndex)
{
    KernelHeader kernel{4};
    kernel.value = data[dataIndex];
    ++dataIndex;
    auto secondWord = data[dataIndex];
    kernel.workGroupSize[0] = static_cast<uint16_t>(secondWord & 0xFFFFU);
    kernel.workGroupSize[1] = static_cast<uint16_t>((secondWord >> 16) & 0xFFFFU);
    kernel.workGroupSize[2] = static_cast<uint16_t>((secondWord >> 32) & 0xFFFFU);
    kernel.workItemMergeFactor = static_cast<uint8_t>((secondWord >> 48) & 0xFFU);
    ++dataIndex;
    auto thirdWord = data[dataIndex];
    kernel.uniformsUsed.value = static_cast<uint32_t>(thirdWord & 0xFFFFFFFFU);
    ++dataIndex;
    kernel.name = readString(data, dataIndex, kernel.getNameLength());
    while(kernel.parameters.size() < kernel.getParamCount())
        kernel.parameters.push_back(ParamHeader::fromBinaryData(data, dataIndex));
    return kernel;
}

const uint32_t ModuleHeader::QPUASM_MAGIC_NUMBER = 0xDEADBEAF;

std::string ModuleHeader::to_string() const
{
    return "Module with " + std::to_string(getKernelCount()) + " kernels, global data with " +
        std::to_string(getGlobalDataSize()) + " words (64-bit each), starting at offset " +
        std::to_string(getGlobalDataOffset()) + " words and " + std::to_string(getStackFrameSize()) +
        " words of stack-frame and kernels:" + ::toString<KernelHeader>(kernels, "\n");
}

std::vector<uint64_t> ModuleHeader::toBinaryData(const std::vector<uint64_t>& globalData, uint16_t numStackWords)
{
    std::vector<uint64_t> result;
    result.reserve(kernels.size() * 16 + globalData.size() + numStackWords);
    result.push_back(uint64_t{QPUASM_MAGIC_NUMBER} | (uint64_t{QPUASM_MAGIC_NUMBER} << uint64_t{32}));
    // insert dummy 64-bit word to be updated later
    result.push_back(0);

    for(const auto& kernel : kernels)
        kernel.toBinaryData(result);

    // write kernel-header-to-global-data delimiter
    result.push_back(0);
    setGlobalDataOffset(result.size());
    result.insert(result.end(), globalData.begin(), globalData.end());
    setStackFrameSize(numStackWords);
    result.insert(result.end(), numStackWords, uint64_t{});
    setGlobalDataSize(result.size() - getGlobalDataOffset());

    // write global-data-to-kernel-instructions delimiter
    result.push_back(0);

    // we need to rewrite the values we just updated
    result[1] = value;
    return result;
}

ModuleHeader ModuleHeader::fromBinaryData(const std::vector<uint64_t>& data)
{
    std::size_t dataIndex = 1; // skip magic number
    ModuleHeader module{data[dataIndex]};
    ++dataIndex;
    while(module.kernels.size() < module.getKernelCount())
        module.kernels.push_back(KernelHeader::fromBinaryData(data, dataIndex));
    return module;
}
