/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "emulator.h"

#include "../common.h"
#ifdef COMPILER_HEADER
#define CPPLOG_NAMESPACE logging
#include COMPILER_HEADER
#endif

#include <algorithm>
#include <array>
#include <fstream>
#include <numeric>

using namespace vc4cl;

// TODO use for scan-build/valgrind!! analysis of tests
// TODO emulate OpenCL-CTS with asan?

// in combination with truncating the 2 upper bits in V3D::busAddressToPhysicalAddress, this leaves us with 128
// possible buffers of 8 MB each
static constexpr uint32_t INDEX_OFFSET = 23;
static constexpr uint32_t ADDRESS_MASK = (1 << INDEX_OFFSET) - 1;
static std::array<std::vector<uint8_t>, 1 << (30 - INDEX_OFFSET)> allocatedMemory;
static std::mutex memoryLock;

struct LeakCheck
{
    ~LeakCheck()
    {
        std::lock_guard<std::mutex> guard(memoryLock);
        for(unsigned i = 0; i < allocatedMemory.size(); ++i)
        {
            if(!allocatedMemory[i].empty())
                std::cerr << "Device-Buffer is not freed: 0x" << std::hex << (i << INDEX_OFFSET) << std::dec << " with "
                          << allocatedMemory[i].size() << " bytes" << std::endl;
        }
    }
};

// this variable is deininitialzed on program exit before allocatedMemory and therefore can check for still alive
// buffers
static LeakCheck leakCheck;

uint32_t vc4cl::getTotalEmulatedMemory()
{
    // to be on the safe side not to overflow on buffers
    return 1 << INDEX_OFFSET;
}

/**
 * Since we always read a whole L2 cache line, we need to round the buffer size to the full cache-line sizes.
 *
 * For the actual hardware, this is not a problem, since the L2 cache just then reads some random memory contents (which
 * should never be accessed). But since our emulator checks every address loaded whether it is properly allocated, it
 * would otherwise fail here.
 */
static uint32_t roundToCacheLineSize(uint32_t numBytes)
{
    static constexpr uint32_t L2_CACHE_LINE_SIZE = 16u * sizeof(uint32_t);
    auto numLines = numBytes / L2_CACHE_LINE_SIZE;
    if(numBytes % L2_CACHE_LINE_SIZE != 0)
        ++numLines;
    // In theory we should be able to round to a single cache line, but in some cases (esp. for 64-bit reads), we
    // preload more memory than a single cache line. Example: vload8 allocates 8 * 8 byte = a single cache line
    // (assuming proper alignment). But since we (out of simplicity) via the TMU load 16 elements (2 * 16 words with 8
    // byte stride), we read past the allocated buffer. In the worst case (where the address loaded by element 0 is not
    // properly aligned), we load up to 120 (128 byte - first 2 words) after our buffer end (i.e. for single/2-element
    // vector 64-bit load), so we reserve two single cache lines more than required.
    numLines += 2;

    return numLines * L2_CACHE_LINE_SIZE;
}

std::unique_ptr<DeviceBuffer> vc4cl::allocateEmulatorBuffer(
    const std::shared_ptr<SystemAccess>& system, unsigned sizeInBytes)
{
    std::lock_guard<std::mutex> guard(memoryLock);
    // 1. allocate
    // look for the first empty buffer and re-use
    auto it = std::find(allocatedMemory.begin(), allocatedMemory.end(), std::vector<uint8_t>{});
    if(it == allocatedMemory.end())
        return nullptr;
    *it = std::vector<uint8_t>(roundToCacheLineSize(sizeInBytes), 0);
    // cannot start with index 0, since 0 is considered an error
    auto handle = static_cast<uint32_t>(it - allocatedMemory.begin()) + 1u;

    // 2. lock
    // convert the index + 1 from allocate to correct index
    // also use high offset to identify the buffer from the address
    DevicePointer qpuPointer{(handle - 1) << INDEX_OFFSET};

    // 3. get host pointer
    auto hostPointer = it->data();
    return std::make_unique<DeviceBuffer>(system, handle, qpuPointer, hostPointer, sizeInBytes);
}

bool vc4cl::deallocateEmulatorBuffer(const DeviceBuffer* buffer)
{
    std::lock_guard<std::mutex> guard(memoryLock);
    // uses index + 1, see allocateBuffer()
    allocatedMemory[buffer->memHandle - 1].clear();
    return true;
}

uint32_t vc4cl::getEmulatedSystemQuery(SystemQuery query)
{
    switch(query)
    {
    case SystemQuery::CURRENT_QPU_CLOCK_RATE_IN_HZ:
    case SystemQuery::MAXIMUM_QPU_CLOCK_RATE_IN_HZ:
        return 250 * 1000 * 1000;
    case SystemQuery::CURRENT_ARM_CLOCK_RATE_IN_HZ:
    case SystemQuery::MAXIMUM_ARM_CLOCK_RATE_IN_HZ:
        return 1000 * 1000 * 1000;
    case SystemQuery::NUM_QPUS:
        return 12;
    case SystemQuery::QPU_TEMPERATURE_IN_MILLI_DEGREES:
        return 25 * 1000;
    case SystemQuery::TOTAL_ARM_MEMORY_IN_BYTES:
    case SystemQuery::TOTAL_GPU_MEMORY_IN_BYTES:
        return 1024 * 1024 * 1024;
    case SystemQuery::TOTAL_VPM_MEMORY_IN_BYTES:
        return 12 * 1024;
    }
    return 0;
}

#ifdef COMPILER_HEADER
static void dumpEmulationLog(std::string&& fileName, std::wistream& logStream)
{
    using namespace vc4cl;
    DEBUG_LOG(DebugLevel::KERNEL_EXECUTION, {
        logStream.seekg(0);
        const std::string dumpFile("/tmp/vc4cl-emulation-" + fileName + "-" + std::to_string(rand()) + ".log");
        std::wofstream f;
        // Dump the emulation log
        std::cout << "Dumping emulation log to " << dumpFile << std::endl;
        f.open(dumpFile, std::ios_base::out | std::ios_base::trunc);
        f << logStream.rdbuf();
    })
}
#endif

bool vc4cl::emulateQPU(unsigned numQPUs, uint32_t bufferQPUAddress, std::chrono::milliseconds timeout)
{
#ifdef COMPILER_HEADER
    auto bufferIndex = bufferQPUAddress >> INDEX_OFFSET;
    auto controlOffset = bufferQPUAddress & ADDRESS_MASK;
    std::wstringstream logStream;
    vc4c::setLogger(logStream, false, vc4c::LogLevel::DEBUG);
    static const auto toDevicePointer = [](uint32_t ptr) -> uint32_t { return (ptr & ~0xC0000000); };

    auto& buffer = allocatedMemory.at(bufferIndex);
    auto controlPtr = reinterpret_cast<uint32_t*>(buffer.data()) + (controlOffset / sizeof(uint32_t));

    std::vector<uint32_t> uniformAddresses;
    uint64_t* kernelPtr = nullptr;
    uint32_t numInstructions = 0;
    for(unsigned i = 0; i < numQPUs; ++i)
    {
        // addresses are (UNIFORM, START) pairs
        uniformAddresses.emplace_back(toDevicePointer(controlPtr[i * 2]));
        // This should be the same for all executions anyway
        if(i == 0)
        {
            auto kernelAddress = toDevicePointer(controlPtr[i * 2 + 1]);
            numInstructions = static_cast<uint32_t>((uniformAddresses.front() - kernelAddress) / sizeof(uint64_t));
            kernelPtr = reinterpret_cast<uint64_t*>(buffer.data()) +
                ((kernelAddress - (bufferIndex << INDEX_OFFSET)) / sizeof(uint64_t));
        }
    }

    std::map<uint32_t, std::reference_wrapper<std::vector<uint8_t>>> buffers;
    for(unsigned i = 0; i < allocatedMemory.size(); ++i)
    {
        if(!allocatedMemory[i].empty())
            buffers.emplace(i << INDEX_OFFSET, allocatedMemory[i]);
    }

    try
    {
        // With 250MHz, the hardware would execute 250k instructions per ms
        auto numCycles = static_cast<uint32_t>(timeout.count() * 250000);
        vc4c::tools::LowLevelEmulationData data(buffers, kernelPtr, numInstructions, uniformAddresses, numCycles);
        using namespace vc4cl;
        DEBUG_LOG(DebugLevel::KERNEL_EXECUTION,
            data.instrumentationDump = "/tmp/vc4cl-instrumentation-" + std::to_string(rand()) + ".log")
        auto res = vc4c::tools::emulate(data);

        dumpEmulationLog(std::to_string(bufferIndex), logStream);
        return res.executionSuccessful;
    }
    catch(const std::exception& err)
    {
        std::cerr << "Error in emulating kernel execution: " << std::endl;
        std::wcerr << logStream.rdbuf();
        std::cerr << err.what() << std::endl;
        dumpEmulationLog(std::to_string(bufferIndex), logStream);
        return false;
    }
#else
    return false;
#endif
}
