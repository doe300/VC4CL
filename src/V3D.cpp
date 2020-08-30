/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "V3D.h"

#include "hal.h"

#include <cerrno>
#include <cstdio>
#include <iostream>
#include <memory>
#include <system_error>

using namespace vc4cl;

static const uint32_t V3D_BASE_OFFSET = 0x00c00000;

V3D::V3D()
{
    bcm_host_init();
    v3dBasePointer = static_cast<uint32_t*>(
        mapmem(busAddressToPhysicalAddress(bcm_host_get_peripheral_address() + V3D_BASE_OFFSET), V3D_LENGTH));
    DEBUG_LOG(DebugLevel::SYSCALL, std::cout << "[VC4CL] V3D base: " << v3dBasePointer << std::endl)
}

V3D::~V3D()
{
    unmapmem(v3dBasePointer, V3D_LENGTH);
    bcm_host_deinit();
}

std::shared_ptr<V3D>& V3D::instance()
{
    static std::shared_ptr<V3D> singleton(new V3D());
    return singleton;
}

uint32_t V3D::getSystemInfo(const SystemInfo key) const
{
    switch(key)
    {
    case SystemInfo::VPM_MEMORY_SIZE:
        if((v3d_register(v3dBasePointer, V3D_IDENT1) >> 28) == 0)
            // 0 => 16K
            return 16 * 1024;
        return (v3d_register(v3dBasePointer, V3D_IDENT1) >> 28) * 1024;
    case SystemInfo::VPM_USER_MEMORY_SIZE:
        //"Contains the amount of VPM memory reserved for all user programs, in multiples of 256 bytes (4x 16-way 32-bit
        // vectors)."
        return (v3d_register(v3dBasePointer, V3D_VPMBASE) & 0x1F) * 256;
    case SystemInfo::SEMAPHORES_COUNT:
        // FIXME (similar to error with QPU count), this returns 0 but should return 16
        // return (v3d_register(v3dBasePointer, V3D_IDENT1) >> 16) & 0xFF;
        return 16;
    case SystemInfo::SLICE_TMU_COUNT:
        return (v3d_register(v3dBasePointer, V3D_IDENT1) >> 12) & 0xF;
    case SystemInfo::SLICE_QPU_COUNT:
        return (v3d_register(v3dBasePointer, V3D_IDENT1) >> 8) & 0xF;
    case SystemInfo::SLICES_COUNT:
        return (v3d_register(v3dBasePointer, V3D_IDENT1) >> 4) & 0xF;
    case SystemInfo::QPU_COUNT:
        return 12;
        // FIXME somehow from time to time (e.g. every first call after reboot??) this returns 196, but why??
        // return ((*(uint32_t*)(mmap_ptr + V3D_IDENT1) >> 4) & 0xF) * ((*(uint32_t*)(mmap_ptr + V3D_IDENT1) >> 8) &
        // 0xF);
    case SystemInfo::HDR_SUPPORT:
        return (v3d_register(v3dBasePointer, V3D_IDENT1) >> 24) & 0x1;
    case SystemInfo::V3D_REVISION:
        return v3d_register(v3dBasePointer, V3D_IDENT1) & 0xF;
    case SystemInfo::USER_PROGRAMS_COMPLETED_COUNT:
        return (v3d_register(v3dBasePointer, V3D_SRQCS) >> 16) & 0xFF;
    case SystemInfo::USER_REQUESTS_COUNT:
        return (v3d_register(v3dBasePointer, V3D_SRQCS) >> 8) & 0xFF;
    case SystemInfo::PROGRAM_QUEUE_FULL:
        return (v3d_register(v3dBasePointer, V3D_SRQCS) >> 7) & 0x1;
    case SystemInfo::PROGRAM_QUEUE_LENGTH:
        return v3d_register(v3dBasePointer, V3D_SRQCS) & 0x3F;
    }
    return 0;
}

bool V3D::setCounter(uint8_t counterIndex, const CounterType type)
{
    if(static_cast<uint8_t>(counterIndex) > 15)
        return false;
    if(static_cast<uint8_t>(counterIndex) > 29)
        return false;

    // 1. enable counter
    // http://maazl.de/project/vc4asm/doc/VideoCoreIV-addendum.html states (section 10):
    //"You need to set bit 31 (allegedly reserved) to enable performance counters at all."
    v3d_register(v3dBasePointer, V3D_COUNTER_ENABLE) |= (1u << 31) | (1u << counterIndex);
    // 2. set mapping
    v3d_register(v3dBasePointer, V3D_COUNTER_MAPPING_BASE + counterIndex * V3D_COUNTER_INCREMENT) =
        static_cast<uint32_t>(type) & 0x1F;
    // 3. reset counter
    // TODO difference between reset here and reset in resetCounterValue??
    v3d_register(v3dBasePointer, V3D_COUNTER_VALUE_BASE + counterIndex * V3D_COUNTER_INCREMENT) = 0;

    return true;
}

void V3D::resetCounterValue(uint8_t counterIndex)
{
    v3d_register(v3dBasePointer, V3D_COUNTER_CLEAR) = (1 << counterIndex);
}

// https://github.com/jonasarrow/vc4top: "Detected by observation, if the GPU is gated, deadbeef is returned"
// Confirmed this detection
static const uint32_t POWEROFF_VALUE = 0xdeadbeef;

int64_t V3D::getCounter(uint8_t counterIndex) const
{
    uint32_t val = static_cast<uint32_t>(
        v3d_register(v3dBasePointer, V3D_COUNTER_VALUE_BASE + counterIndex * V3D_COUNTER_INCREMENT));
    if(val == POWEROFF_VALUE)
        return static_cast<int64_t>(-1);
    return static_cast<int64_t>(val);
}

void V3D::disableCounter(uint8_t counterIndex)
{
    resetCounterValue(counterIndex);
    // TODO correct?? the or?
    v3d_register(v3dBasePointer, V3D_COUNTER_ENABLE) |= 0xFFFF ^ (1 << counterIndex);
}

bool V3D::setReservation(const uint8_t qpu, const QPUReservation val)
{
    if(qpu > 16)
        return false;
    // 8 reservation settings are in one 32-bit register (4 bit per setting)
    uint32_t registerOffset = qpu / 8;
    uint32_t bitOffset = (qpu % 8) * 4;
    uint32_t writeVal = (static_cast<uint8_t>(val) & 0xFu) << bitOffset;
    // clear old values
    v3d_register(v3dBasePointer, V3D_QPU_RESERVATIONS0 + registerOffset) &= ~(0xFu << bitOffset);
    // set new values
    v3d_register(v3dBasePointer, V3D_QPU_RESERVATIONS0 + registerOffset) |= writeVal;

    return true;
}

QPUReservation V3D::getReservation(const uint8_t qpu) const
{
    uint32_t registerOffset = qpu / 8;
    uint32_t bitOffset = (qpu % 8) * 4;
    uint32_t rawValue = v3d_register(v3dBasePointer, V3D_QPU_RESERVATIONS0 + registerOffset) >> bitOffset;
    return static_cast<QPUReservation>(rawValue & 0xF);
}

bool V3D::hasError(const ErrorType type) const
{
    // read bit
    uint32_t val = v3d_register(v3dBasePointer, V3D_ERRORS) >> static_cast<uint8_t>(type);
    // reset bit
    v3d_register(v3dBasePointer, V3D_ERRORS) = 1 << static_cast<uint8_t>(type);
    return val == 1;
}

ExecutionHandle V3D::executeQPU(
    unsigned numQPUs, std::pair<uint32_t*, unsigned> addressPairs, bool flushBuffer, std::chrono::milliseconds timeout)
{
    // see
    // https://github.com/raspberrypi/userland/blob/master/host_applications/linux/apps/hello_pi/hello_fft/gpu_fft_base.c,
    // function gpu_fft_base_exec_direct
    // TODO interrupts?? not in Broadcom spec
    // https://vc4-notes.tumblr.com/post/125039428234/v3d-registers-not-on-videocore-iv-3d-architecture
    // see errata: https://elinux.org/VideoCore_IV_3D_Architecture_Reference_Guide_errata

    // clear cache (if set)
    // FIXME when the buffer-flush is disabled (for any consecutive execution), the updated UNIFORM-values are not used,
    // but the old ones!
    //-> which results in incorrect executions (except the first one)
    // XXX can this be re-enabled, if we allocate the host-pointer (Mailbox.cpp) with direct access-mode?
    // if(flushBuffer)
    {
        // clear L2 cache
        v3d_register(v3dBasePointer, V3D_L2CACTL) = 1 << 2;
        // clear uniforms and instructions caches (TMU too)
        v3d_register(v3dBasePointer, V3D_SLCACTL) = 0xFFFFFFFF;
    }

    // reset user program states
    v3d_register(v3dBasePointer, V3D_SRQCS) = (1 << 7) | (1 << 8) | (1 << 16);

    // write uniforms and instructions addresses for all QPUs
    uint32_t* addressBase = addressPairs.first;
    for(unsigned i = 0; i < numQPUs; ++i)
    {
        v3d_register(v3dBasePointer, V3D_SRQUA) = addressBase[0];
        v3d_register(v3dBasePointer, V3D_SRQPC) = addressBase[1];
        addressBase += 2;
    }

    const auto start = std::chrono::high_resolution_clock::now();
    auto basePointer = v3dBasePointer;
    auto checkFunc = [basePointer, numQPUs, start, timeout]() -> bool {
        // wait for completion
        while(true)
        {
            if(((v3d_register(basePointer, V3D_SRQCS) >> 16) & 0xFF) == numQPUs)
                return true;
            if(std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::high_resolution_clock::now() - start) > timeout)
                break;
            // TODO sleep some time?? so CPU is not fully used for waiting
            // e.g. sleep for the theoretical execution time of the kernel (e.g. #instructions / QPU clock) and then
            // begin active waiting
        }
        return false;
    };
    return ExecutionHandle{checkFunc};
}

uint32_t V3D::busAddressToPhysicalAddress(uint32_t busAddress)
{
    return busAddress & ~0xC0000000;
}

void* vc4cl::mapmem(unsigned base, unsigned size)
{
    // TODO this is called once to initialize V3D and every time a buffer is mapped.
    // Can't we cache the memory fd?
    int mem_fd;
    unsigned offset = base % V3D::MEMORY_PAGE_SIZE;
    base = base - offset;
    /*
    size = size + offset;
    */
    /* open /dev/mem */
    if((mem_fd = open_memory()) < 0)
    {
        std::cout << "[VC4CL] can't open /dev/mem" << std::endl;
        std::cout << "[VC4CL] This program should be run as root. Try prefixing command with: sudo" << std::endl;
        throw std::system_error(errno, std::system_category(), "Failed to open /dev/mem");
    }
    void* mem = map_memory(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED /*|MAP_FIXED*/, mem_fd, base);
    DEBUG_LOG(DebugLevel::SYSCALL, printf("[VC4CL] base=0x%x, mem=%p\n", base, mem))
    if(mem == MAP_FAILED)
    {
        std::cout << "[VC4CL] mmap error " << mem << std::endl;
        perror("[VC4CL] Error in mapmem");
        close_memory(mem_fd);
        throw std::system_error(errno, std::system_category(), "Error in mapmem");
    }
    close_memory(mem_fd);
    return reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(mem) + offset);
}

void vc4cl::unmapmem(void* addr, unsigned size)
{
    /*
    const intptr_t offset = (intptr_t)addr % V3D::MEMORY_PAGE_SIZE;
    addr = (char *)addr - offset;
    size = size + offset;
    */
    int s = unmap_memory(addr, size);
    if(s != 0)
    {
        std::cout << "munmap error " << s << std::endl;
        perror("Error in unmapmem");
        throw std::system_error(errno, std::system_category(), "Error in unmapmem");
    }
}
