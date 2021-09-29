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

template <typename T>
static void writeByteContainer(const T& input, std::vector<uint64_t>& data)
{
    static_assert(sizeof(typename T::value_type) == 1, "Value type must be a single byte!");
    auto numWords = input.size() / sizeof(uint64_t);
    numWords += (input.size() % sizeof(uint64_t) != 0) ? 1 : 0;
    auto currentIndex = data.size();
    data.resize(data.size() + numWords);
    std::copy(input.begin(), input.end(), reinterpret_cast<typename T::value_type*>(&data[currentIndex]));
}

static auto writeString = writeByteContainer<std::string>;

template <typename T>
static T readByteContainer(const std::vector<uint64_t>& data, std::size_t& dataIndex, std::size_t numBytes)
{
    static_assert(sizeof(typename T::value_type) == 1, "Value type must be a single byte!");
    T result(reinterpret_cast<const typename T::value_type*>(&data[dataIndex]),
        reinterpret_cast<const typename T::value_type*>(&data[dataIndex]) + numBytes);
    dataIndex += numBytes / sizeof(uint64_t);
    dataIndex += (numBytes % sizeof(uint64_t) != 0) ? 1 : 0;
    return result;
}

static auto readString = readByteContainer<std::string>;

MetaData::Type MetaData::getType() const
{
    if(payload.empty())
        throw std::runtime_error{"Metadata has no payload and therefore no type!"};
    return static_cast<Type>(payload[2]);
}

std::string MetaData::to_string(bool withQuotes) const
{
    std::string tmp;
    switch(getType())
    {
    case Type::KERNEL_VECTOR_TYPE_HINT:
        tmp = "vec_type_hint(" + getString() + ")";
        break;
    case Type::KERNEL_WORK_GROUP_SIZE:
    {
        auto sizes = getSizes();
        tmp = "reqd_work_group_size(" + std::to_string(sizes[0]) + ", " + std::to_string(sizes[1]) + ", " +
            std::to_string(sizes[2]) + ")";
        break;
    }
    case Type::KERNEL_WORK_GROUP_SIZE_HINT:
    {
        auto sizes = getSizes();
        tmp = "work_group_size_hint(" + std::to_string(sizes[0]) + ", " + std::to_string(sizes[1]) + ", " +
            std::to_string(sizes[2]) + ")";
        break;
    }
    }
    return withQuotes ? "\"" + tmp + "\"" : tmp;
}

void MetaData::toBinaryData(std::vector<uint64_t>& data) const
{
    writeByteContainer(payload, data);
}

MetaData MetaData::fromBinaryData(const std::vector<uint64_t>& data, std::size_t& dataIndex)
{
    MetaData metaData;
    auto numBytes = data[dataIndex] & 0xFFFFU;
    metaData.payload = readByteContainer<std::vector<uint8_t>>(data, dataIndex, numBytes);
    return metaData;
}

std::string MetaData::getString() const
{
    auto numBytes = static_cast<uint16_t>(payload[0]) + (static_cast<uint16_t>(payload[1]) << uint16_t{8});
    auto start = reinterpret_cast<const char*>(payload.data());
    return std::string(start + 3u /* length + type */, start + numBytes);
}

void MetaData::setString(Type type, const std::string& text)
{
    if(text.size() > std::numeric_limits<uint16_t>::max())
        throw std::invalid_argument{"Text is too big to fit into a metadata entry!"};

    payload.clear();
    auto numBytes = text.size() + 3u /* length + type */;
    payload.reserve(numBytes);
    payload.push_back(static_cast<uint8_t>(numBytes & 0xFF));
    payload.push_back(static_cast<uint8_t>((numBytes >> 8) & 0xFF));
    payload.push_back(static_cast<uint8_t>(type));
    payload.insert(payload.end(), text.begin(), text.end());
}

std::array<uint32_t, 3> MetaData::getSizes() const
{
    std::array<uint32_t, 3> result{};
    result[0] = static_cast<uint32_t>(payload[4]) | (static_cast<uint32_t>(payload[5]) << 8u) |
        (static_cast<uint32_t>(payload[6]) << 16u) | (static_cast<uint32_t>(payload[7]) << 24u);
    result[1] = static_cast<uint32_t>(payload[8]) | (static_cast<uint32_t>(payload[9]) << 8u) |
        (static_cast<uint32_t>(payload[10]) << 16u) | (static_cast<uint32_t>(payload[11]) << 24u);
    result[2] = static_cast<uint32_t>(payload[12]) | (static_cast<uint32_t>(payload[13]) << 8u) |
        (static_cast<uint32_t>(payload[14]) << 16u) | (static_cast<uint32_t>(payload[15]) << 24u);
    return result;
}

void MetaData::setSizes(Type type, const std::array<uint32_t, 3>& sizes)
{
    payload.resize(16);
    payload[0] = 16; // lower size
    payload[1] = 0;  // upper size
    payload[2] = static_cast<uint8_t>(type);
    payload[3] = 0; // padding
    payload[4] = static_cast<uint8_t>(sizes[0] & 0xFF);
    payload[5] = static_cast<uint8_t>((sizes[0] >> 8u) & 0xFF);
    payload[6] = static_cast<uint8_t>((sizes[0] >> 16u) & 0xFF);
    payload[7] = static_cast<uint8_t>((sizes[0] >> 24u) & 0xFF);
    payload[8] = static_cast<uint8_t>(sizes[1] & 0xFF);
    payload[9] = static_cast<uint8_t>((sizes[1] >> 8u) & 0xFF);
    payload[10] = static_cast<uint8_t>((sizes[1] >> 16u) & 0xFF);
    payload[11] = static_cast<uint8_t>((sizes[1] >> 24u) & 0xFF);
    payload[12] = static_cast<uint8_t>(sizes[2] & 0xFF);
    payload[13] = static_cast<uint8_t>((sizes[2] >> 8u) & 0xFF);
    payload[14] = static_cast<uint8_t>((sizes[2] >> 16u) & 0xFF);
    payload[15] = static_cast<uint8_t>((sizes[2] >> 24u) & 0xFF);
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

    auto metaString = metaData.empty() ? "" : (" (metadata: " + ::toString<MetaData>(metaData) + ")");

    return std::string("Kernel '") + (name + "' with ") + (std::to_string(getLength()) + " instructions, offset ") +
        (std::to_string(getOffset()) + ", with following parameters: ") + ::toString<ParamHeader>(parameters) +
        uniformsString + mergeFactor + metaString;
}
LCOV_EXCL_STOP

void KernelHeader::toBinaryData(std::vector<uint64_t>& data) const
{
    data.push_back(value);
    auto secondWord = uint64_t{workGroupSize[0]} | (uint64_t{workGroupSize[1]} << uint64_t{16}) |
        (uint64_t{workGroupSize[2]} << uint64_t{32}) | (uint64_t{workItemMergeFactor} << uint64_t{48});
    data.push_back(secondWord);
    auto thirdWord = static_cast<uint64_t>(uniformsUsed.value) |
        (static_cast<uint64_t>(metaData.size() & 0xFFFFFFFFU) << uint64_t{32});
    data.push_back(thirdWord);
    writeString(name, data);
    for(const auto& param : parameters)
        param.toBinaryData(data);
    for(const auto& meta : metaData)
        meta.toBinaryData(data);
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
    auto numMetaDataEntries = static_cast<uint32_t>((thirdWord >> 32) & 0xFFFFFFFFU);
    ++dataIndex;
    kernel.name = readString(data, dataIndex, kernel.getNameLength());
    while(kernel.parameters.size() < kernel.getParamCount())
        kernel.parameters.push_back(ParamHeader::fromBinaryData(data, dataIndex));
    while(kernel.metaData.size() < numMetaDataEntries)
        kernel.metaData.push_back(MetaData::fromBinaryData(data, dataIndex));
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
