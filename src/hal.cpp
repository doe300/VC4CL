/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "hal.h"

#include "Mailbox.h"
#include "common.h"
#ifdef COMPILER_HEADER
#define CPPLOG_NAMESPACE logging
#include COMPILER_HEADER
#endif

#include <algorithm>
#include <array>
#include <fstream>
#include <numeric>

#if defined(MOCK_HAL) && MOCK_HAL

// TODO use for scan-build/valgrind!! analysis of tests
// TODO emulate OpenCL-CTS with asan?

static bool isQPUEnabled = false;

// in combination with truncating the 2 upper bits in V3D::busAddressToPhysicalAddress, this leaves us with 128
// possible buffers of 8 MB each
static constexpr uint32_t INDEX_OFFSET = 23;
static constexpr uint32_t ADDRESS_MASK = (1 << INDEX_OFFSET) - 1;
static std::array<std::vector<uint8_t>, 1 << (30 - INDEX_OFFSET)> allocatedMemory;
static std::mutex memoryLock;
static vc4cl::WordWrapper completedPrograms{0, [](uint32_t val) -> uint32_t {
                                                // XXX actually only some values clear the counter(s!), but we always
                                                // write these values anyway
                                                return 0;
                                            }};
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

void bcm_host_init()
{
    // no-op
}

void bcm_host_deinit()
{
    // no-op
}

unsigned bcm_host_get_peripheral_address()
{
    // no-op
    return 0xDEADBEEF;
}

int vc4cl::open_mailbox(const char* path, int flags)
{
    // no-op
    return 0xBEEF;
}

int vc4cl::close_mailbox(int fd)
{
    // no-op
    return 0;
}

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

static bool emulateQPU(unsigned numQPUs, uint32_t bufferIndex, uint32_t controlOffset, uint32_t timeoutInMs)
{
    std::wstringstream logStream;
    vc4c::setLogger(logStream, false, vc4c::LogLevel::DEBUG);
    // XXX mod 256
    completedPrograms.word += (1 << 8);
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
        auto numCycles = timeoutInMs * 250000;
        vc4c::tools::LowLevelEmulationData data(buffers, kernelPtr, numInstructions, uniformAddresses, numCycles);
        using namespace vc4cl;
        DEBUG_LOG(DebugLevel::KERNEL_EXECUTION,
            data.instrumentationDump = "/tmp/vc4cl-instrumentation-" + std::to_string(rand()) + ".log")
        auto res = vc4c::tools::emulate(data);
        // XXX mod 256
        completedPrograms.word += (1 << 16);

        dumpEmulationLog(std::to_string(bufferIndex), logStream);
        return res.executionSuccessful;
    }
    catch(const vc4c::CompilationError& err)
    {
        std::cerr << "Error in emulating kernel execution: " << std::endl;
        std::wcerr << logStream.rdbuf();
        std::cerr << err.what() << std::endl;
        dumpEmulationLog(std::to_string(bufferIndex), logStream);
        return false;
    }
}

int vc4cl::ioctl_mailbox(int fd, unsigned long request, void* data)
{
    /*
     * 0: Buffer size
     * 1: request/response code
     * 2: request tag
     * 3: content size
     * 4: request/response data size
     * ...: request/response data
     * x: end tag (0)
     *
     */
    auto buffer = reinterpret_cast<uint32_t*>(data);
    switch(static_cast<MailboxTag>(buffer[2]))
    {
    case MailboxTag::ALLOCATE_MEMORY:
        buffer[4] = 4;
        if((buffer[5] & ADDRESS_MASK) != buffer[5])
            // buffer too big
            buffer[5] = 0;
        else
        {
            std::lock_guard<std::mutex> guard(memoryLock);
            // look for the first empty buffer and re-use
            auto it = std::find(allocatedMemory.begin(), allocatedMemory.end(), std::vector<uint8_t>{});
            if(it != allocatedMemory.end())
            {
                *it = std::vector<uint8_t>(buffer[5], 0);
                // cannot start with index 0, since 0 is considered an error
                buffer[5] = static_cast<uint32_t>(it - allocatedMemory.begin()) + 1u;
            }
            else
                // no more free buffers
                buffer[5] = 0;
        }
        break;
    case MailboxTag::ARM_MEMORY:
        buffer[5] = 0;
        buffer[6] = 0xFFFFFFFF;
        break;
    case MailboxTag::BOARD_MODEL:
        buffer[5] = 0xDEADBEEF;
        break;
    case MailboxTag::BOARD_REVISION:
        buffer[5] = 0xDEADBEEF;
        break;
    case MailboxTag::BOARD_SERIAL:
        buffer[4] = 8;
        buffer[5] = 0xDEADBEEF;
        buffer[6] = 0xCAFFEE11;
        break;
    case MailboxTag::ENABLE_QPU:
        isQPUEnabled = buffer[5] != 0;
        buffer[5] = 0;
        break;
    case MailboxTag::EXECUTE_QPU:
        buffer[4] = 4;
        // "device address" is: (buffer-index << 26) + offset
        buffer[5] = emulateQPU(buffer[5], buffer[6] >> INDEX_OFFSET, buffer[6] & ADDRESS_MASK, buffer[8]) ? 0 : 1;
        break;
    case MailboxTag::FIRMWARE_REVISION:
        buffer[5] = 0xDEADBEEF;
        break;
    case MailboxTag::GET_CLOCK_RATE:
    case MailboxTag::GET_MAX_CLOCK_RATE:
    case MailboxTag::GET_MIN_CLOCK_RATE:
    case MailboxTag::GET_TEMPERATURE:
    case MailboxTag::GET_MAX_TEMPERATURE:
    case MailboxTag::GET_VOLTAGE:
    case MailboxTag::GET_MAX_VOLTAGE:
    case MailboxTag::GET_MIN_VOLTAGE:
        buffer[4] = 8;
        buffer[6] = 250000000;
        break;
    case MailboxTag::LOCK_MEMORY:
        // convert the index + 1 from allocate to correct index
        // also use high offset to identify the buffer from the address
        buffer[5] = (buffer[5] - 1) << INDEX_OFFSET;
        break;
    case MailboxTag::MAC_ADDRESS:
        buffer[4] = 6;
        buffer[5] = 0xDEADBEEF;
        buffer[6] = 0xCAFFEE11;
        break;
    case MailboxTag::RELEASE_MEMORY:
    {
        std::lock_guard<std::mutex> guard(memoryLock);
        // uses index + 1, see ALLOCATE_MEMORY
        allocatedMemory[buffer[5] - 1].clear();
        buffer[5] = 0;
        break;
    }
    case MailboxTag::UNLOCK_MEMORY:
        buffer[5] = 0;
        break;
    case MailboxTag::VC_MEMORY:
        buffer[5] = 0;
        // to be on the safe side not to overflow on buffers
        buffer[6] = 1 << INDEX_OFFSET;
        break;
    default:
        throw std::invalid_argument("Unhandled Mailbox tag!" + std::to_string(buffer[2]));
    }
    buffer[1] |= 0x80000000;
    return 0;
}

int vc4cl::open_memory()
{
    // no-op
    return 0xDEAD;
}

int vc4cl::close_memory(int fd)
{
    // no-op
    return 0;
}

void* vc4cl::map_memory(void* addr, size_t len, int prot, int flags, int fd, off_t offset)
{
    // for the V3D base address, it doesn't matter what is returned, for the mailbox buffer, this is correct.
    // The offset is the result of lock, which is shifted, so we need to shift it back to get the actual index
    if(addr == nullptr && (static_cast<size_t>(offset) >> INDEX_OFFSET) < allocatedMemory.size())
        return allocatedMemory[static_cast<size_t>(offset) >> INDEX_OFFSET].data();
    return addr;
}

int vc4cl::unmap_memory(void* addr, size_t len)
{
    // no-op
    return 0;
}

vc4cl::WordWrapper& vc4cl::v3d_register(uint32_t* basePtr, uint32_t offset)
{
    static auto writeNotSupported = [](uint32_t val) -> uint32_t {
        throw std::runtime_error("Writing this value is not supported!");
    };
    // Broadcom specification, table 62
    static WordWrapper SQRSV0{0, writeNotSupported};
    // Broadcom specification, table 63
    static WordWrapper SQRSV1{0, writeNotSupported};
    // Broadcom specification, table 65
    static WordWrapper SRQPC{0, writeNotSupported};
    // Need to queue up all executions, since e.g. semaphores disallow running the work-items sequentially
    // -> currently not supported
    // Broadcom specification, table 66
    static WordWrapper SRQUA{0, writeNotSupported};
    // Broadcom specification, table 67
    static WordWrapper SRQUL{0, writeNotSupported};
    // Broadcom specification, table 68
    static WordWrapper& SRQCS = completedPrograms;
    // Broadcom specification, table 69
    static WordWrapper VPMBASE{0, writeNotSupported};
    // Broadcom specification, table 71
    static WordWrapper L2CACTL{0};
    // Broadcom specification, table 72
    static WordWrapper SLCACTL{0};
    // Broadcom specification, table 63
    static WordWrapper DBQITC{0};
    // Broadcom specification, table 73
    static WordWrapper DBQITE{0};
    // Broadcom specification, table 79
    static WordWrapper IDENT0{(2 << 24) | ('V' << 16) | ('3' << 8) | 'D', writeNotSupported};
    // Broadcom specification, table 80
    static WordWrapper IDENT1{(12u << 28) | (16u << 16) | (2u << 12) | (4u << 8) | (3 << 4) | 1u, writeNotSupported};
    // Broadcom specification, table 83
    static WordWrapper PCTRC{0};
    // Broadcom specification, table 84
    static WordWrapper PCTRE{0};
    // Broadcom specification, table 85
    static WordWrapper PCTRn{0};
    // Broadcom specification, table 86
    static WordWrapper PCTRSn{0};
    // Broadcom specification, table 87
    static WordWrapper ERRSTAT{0};
    switch(offset)
    {
    case V3D_COUNTER_CLEAR:
        return PCTRC;
    case V3D_COUNTER_ENABLE:
        return PCTRE;
    case V3D_COUNTER_MAPPING_BASE:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 1:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 2:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 3:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 4:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 5:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 6:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 7:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 8:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 9:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 10:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 11:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 12:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 13:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 14:
    case V3D_COUNTER_MAPPING_BASE + V3D_COUNTER_INCREMENT * 15:
        return PCTRSn;
    case V3D_COUNTER_VALUE_BASE:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 1:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 2:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 3:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 4:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 5:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 6:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 7:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 8:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 9:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 10:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 11:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 12:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 13:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 14:
    case V3D_COUNTER_VALUE_BASE + V3D_COUNTER_INCREMENT * 15:
        return PCTRn;
    case V3D_IDENT0:
        return IDENT0;
    case V3D_IDENT1:
        return IDENT1;
    case V3D_ERRORS:
        return ERRSTAT;
    case V3D_L2CACTL:
        return L2CACTL;
    case V3D_SLCACTL:
        return SLCACTL;
    case V3D_QPU_RESERVATIONS0:
        return SQRSV0;
    case V3D_QPU_RESERVATIONS0 + 1:
        return SQRSV1;
    case V3D_SRQPC:
        return SRQPC;
    case V3D_SRQUA:
        return SRQUA;
    case V3D_SRQCS:
        return SRQCS;
    case V3D_VPMBASE:
        return VPMBASE;
    default:
        throw std::invalid_argument("Unhandled V3D address offset!" + std::to_string(offset));
    }
}

#endif