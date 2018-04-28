/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_V3D
#define VC4CL_V3D

#include "common.h"

#include <chrono>
#include <cstdint>
#include <utility>

namespace vc4cl
{
    enum class SystemInfo
    {
        VPM_MEMORY_SIZE,
        VPM_USER_MEMORY_SIZE,
        SEMAPHORES_COUNT,
        SLICE_TMU_COUNT,
        SLICE_QPU_COUNT,
        SLICES_COUNT,
        QPU_COUNT,
        HDR_SUPPORT,
        V3D_REVISION,
        USER_PROGRAMS_COMPLETED_COUNT,
        USER_REQUESTS_COUNT,
        PROGRAM_QUEUE_FULL,
        PROGRAM_QUEUE_LENGTH
    };

    // see official documentation, page 98
    enum class CounterType : unsigned char
    {
        //"QPU Total idle clock cycles for all QPUs"
        IDLE_CYCLES = 13,
        //"QPU Total clock cycles for all QPUs executing valid instructions"
        EXECUTION_CYCLES = 16,
        //"QPU Total clock cycles for all QPUs stalled waiting for TMUs"
        TMU_STALL_CYCLES = 17,
        //"QPU Total instruction cache hits for all slices"
        INSTRUCTION_CACHE_HITS = 20,
        //"QPU Total instruction cache misses for all slices"
        INSTRUCTION_CACHE_MISSES = 21,
        //"QPU Total uniforms cache hits for all slices"
        UNIFORM_CACHE_HITS = 22,
        //"QPU Total uniforms cache misses for all slices"
        UNIFORM_CACHE_MISSES = 23,
        //"VPM Total clock cycles VDW is stalled waiting for VPM access"
        VPW_STALL_CYCES = 26,
        //"VPM Total clock cycles VCD is stalled waiting for VPM access"
        VCD_STALL_CYCLES = 27,
        //"L2C Total Level 2 cache hits"
        L2_CACHE_HITS = 28,
        //"L2C Total Level 2 cache misses"
        L2_CACHE_MISSES = 29
    };

    // see official documentation, page 89
    enum class QPUReservation
    {
        ALLOW_ALL = 0,
        //"Do not use for User Programs"
        NO_USER_PROGRAM = 1,
        //"Do not use for Fragment Shaders"
        NO_FRAGMENT_SHADER = 2,
        //"Do not use for Vertex Shaders"
        NO_VERTEX_SHADER = 4,
        //"Do not use for Coordinate Shaders"
        NO_COORDINATE_SHADER = 8
    };

    // see official Broadcom documentation, page 100
    enum class ErrorType : unsigned char
    {
        //"L2C AXI Receive Fifo Overrun error"
        L2CARE = 15,
        VCD_IDLE = 12,
        //"VCD error - FIFO pointers out of sync"
        VCD_OOS = 11,
        //"VDW error - address overflows"
        VDW_OVERFLOW = 10,
        //"VPM error - allocated size error"
        VPM_SIZE_ERROR = 9,
        // VPM error - free non-allocated"
        VPM_FREE_NONALLOCATED = 8,
        //"VPM error - write non-allocated"
        VPM_WRITE_NONALLOCATED = 7,
        //"VPM error - read non-allocated"
        VPM_READ_NONALLOCATED = 6,
        //"VPM error - read range"
        VPM_READ_RANGE = 5,
        //"VPM error - write range"
        VPM_WRITE_RANGE = 4,
        //"VPM Allocator error - request too big"
        VPM_REQUEST_TOO_BIG = 1,
        //"VPM Allocator error - allocating base while busy"
        VPM_ALLOCATING_WHILE_BUSY = 0
    };

    class V3D
    {
    public:
        static V3D& instance();

        ~V3D();

        uint32_t getSystemInfo(SystemInfo key) const __attribute__((pure));

        CHECK_RETURN bool setCounter(uint8_t counterIndex, CounterType type);
        void resetCounterValue(uint8_t counterIndex);
        int32_t getCounter(uint8_t counterIndex) const __attribute__((pure));
        void disableCounter(uint8_t counterIndex);

        CHECK_RETURN bool setReservation(uint8_t qpu, QPUReservation val);
        QPUReservation getReservation(uint8_t qpu) const __attribute__((pure));

        bool hasError(ErrorType type) const;

        CHECK_RETURN bool executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> addressPairs, bool flushBuffer,
            std::chrono::milliseconds timeout);

        static uint32_t busAddressToPhysicalAddress(uint32_t busAddress) __attribute__((const));
        static constexpr uint32_t MEMORY_PAGE_SIZE = 4 * 1024; // 4 KB

    private:
        V3D();

        uint32_t* v3dBasePointer;
    };

    void* mapmem(unsigned base, unsigned size);
    void unmapmem(void* addr, unsigned size);

} /* namespace vc4cl */

#ifdef __cplusplus
extern "C" {
#endif

// see "/opt/vc/include/bcm_host.h"
void bcm_host_init(void);
void bcm_host_deinit(void);
unsigned bcm_host_get_peripheral_address(void);

#ifdef __cplusplus
}
#endif

#endif /* VC4CL_V3D */
