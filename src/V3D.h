/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_V3D
#define VC4CL_V3D

#include "common.h"
#include "executor.h"

#include <chrono>
#include <cstdint>
#include <memory>
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
        /**
         * "QPU Total instruction cache hits for all slices"
         *
         * NOTE: According to http://imrc.noip.me/blog/vc4/QI3/ this is also incremented for instruction cache misses.
         * This means, that, at least when executing only on a single core, the counter value is equal to the number of
         * instructions executed.
         *
         * Experiments have shown, this calculates as: InstructionCount = #QPUs * #InstructionsPerQPU (for purely linear
         * code where all cores execute the same amount of instructions).
         */
        INSTRUCTION_CACHE_HITS = 20,
        /**
         * "QPU Total instruction cache misses for all slices"
         *
         * According to http://imrc.noip.me/blog/vc4/QI3/ this is ceil(InstructionCount)/8 (on a single QPU, when no
         * branches are executed) which matches the instruction cache size of 64B (8 instructions) and a cache refresh
         * policy always updating whole cache at once.
         *
         * Experiments have shown, this calculates as (ceil(#QPUs / 4) * #InstructionsPerQPU) / 8 (when all QPUs execute
         * the same amount of instructions and there are no branches in the code). The constant 4 seems to be the 4 QPUs
         * per slice in use, since the instruction cache is per slice.
         */
        INSTRUCTION_CACHE_MISSES = 21,
        /**
         * "QPU Total uniforms cache hits for all slices"
         *
         * According to http://imrc.noip.me/blog/vc4/QI3/ the UNIFORM FIFO is always filled when starting executing,
         * even if no UNIFORMs are read. This counter is also counted up on UNIFORM cache misses.
         *
         * The UNIFORMs FIFO seems to hold 8 Bytes (2 UNIFORMs) and values are updated separately, so reading one
         * UNIFORM value from the UNIFORM FIFO into the QPU would cause a single value to be read from the UNIFORM cache
         * into the UNIFORM FIFO.
         *
         * According to http://imrc.noip.me/blog/vc4/QU16/, the UNIFORM cache holds 16 values (64 Byte).
         *
         * Experiments have confirmed, this is #QPUs * (#UNIFORMSPerQPU + 2) assuming all QPUs load the same amount of
         * UNIFORMs.
         *
         * Layout:
         * L2 -> UNIFORM cache (per slice) -> UNIFORM FIFO (per QPU)
         */
        UNIFORM_CACHE_HITS = 22,
        /**
         * "QPU Total uniforms cache misses for all slices"
         *
         * According to http://imrc.noip.me/blog/vc4/QI3/ this is not the number of UNIFORM cache misses, but rather the
         * number of times values are fetched from the L2 cache into the UNIFORM cache. Since the UNIFORM cache always
         * catches 2 values (8B) at once, this value is only incremented for every 2 UNIFORMs read (into FIFO, not
         * necessarily into QPU!).
         *
         * According to http://imrc.noip.me/blog/vc4/QU16/, this results in UNIFORM misses = ceil(UNIFORM hits/16).
         *
         * Experiments have shown, this calculates as (ceil(#QPUs / 4) * (#UNIFORMSPerQPU + 2)) / 16 assuming all QPUs
         * load the exact same UNIFORMs. The constant 4 seems to be the 4 QPUs per slice in use, since the instruction
         * cache is per slice.
         */
        UNIFORM_CACHE_MISSES = 23,
        /**
         * "TMU Total texture quads processed"
         *
         * This is the number of words loaded via the TMU
         *
         * According to http://imrc.noip.me/blog/vc4/QT12 this is incremented by for each SIMD element loaded, so a full
         * 16-element vector load increments this counter by 16.
         *
         * NOTE: Tests have shown that the counter is always incremented by the full 16 elements, even if only some
         * elements are actually loaded.
         *
         * According to http://imrc.noip.me/blog/vc4/QT31/, the TMU cache is direct associative (see
         * https://en.wikipedia.org/wiki/CPU_cache#Associativity).
         *
         * Experiments have confirmed, this value is the total number (all QPUs, both TMUs, all SIMD vector elements) of
         * words (vector elements) loaded: #QPUs * #TMULoadsPerQPU * 16.
         */
        TMU_TOTAL_WORDS = 24,
        /**
         * "TMU Total texture cache misses (number of fetches from memory/L2cache)"
         *
         * According to http://imrc.noip.me/blog/vc4/QT12, fetches from L2 into TMU cache is done in blocks of 64B (16
         * words).
         *
         * Experiments have shown that loading the exact same address across all QPUs (at the same time) only increments
         * the counter once. Also when the same address is loaded with both TMU (at the same time), the counter is
         * incremented twice.
         */
        TMU_CACHE_MISSES = 25,
        //"VPM Total clock cycles VDW is stalled waiting for VPM access"
        VDW_STALL_CYCES = 26,
        //"VPM Total clock cycles VCD is stalled waiting for VPM access"
        VCD_STALL_CYCLES = 27,
        /**
         * "L2C Total Level 2 cache hits"
         *
         * According to http://imrc.noip.me/blog/vc4/QI3/, in contrast to the other caches (e.g. UNIFORM and
         * instruction), this counter is not incremented on L2 cache misses.
         *
         * According to http://imrc.noip.me/blog/vc4/QT31/, the TMU cache is 4-way associative (see
         * https://en.wikipedia.org/wiki/CPU_cache#Associativity).
         */
        L2_CACHE_HITS = 28,
        /**
         * "L2C Total Level 2 cache misses"
         *
         * According to the observations in http://imrc.noip.me/blog/vc4/QI3/, the counter is always incremented when
         * the L2 cache is filled with data from RAM. Also, the L2 cache fetches from RAM in 64Byte lines.
         */
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
        //"VCD Idle"
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
        static std::shared_ptr<V3D>& instance();

        ~V3D();

        uint32_t getSystemInfo(SystemInfo key) const __attribute__((pure));

        CHECK_RETURN bool setCounter(uint8_t counterIndex, CounterType type);
        void resetCounterValue(uint8_t counterIndex);
        int64_t getCounter(uint8_t counterIndex) const __attribute__((pure));
        void disableCounter(uint8_t counterIndex);

        CHECK_RETURN bool setReservation(uint8_t qpu, QPUReservation val);
        QPUReservation getReservation(uint8_t qpu) const __attribute__((pure));

        bool hasError(ErrorType type) const;

        CHECK_RETURN ExecutionHandle executeQPU(unsigned numQPUs, std::pair<uint32_t*, unsigned> addressPairs,
            bool flushBuffer, std::chrono::milliseconds timeout);

        static uint32_t busAddressToPhysicalAddress(uint32_t busAddress) __attribute__((const));
        static constexpr uint32_t MEMORY_PAGE_SIZE = 4 * 1024; // 4 KB

    private:
        V3D();

        uint32_t* v3dBasePointer;
    };

    void* mapmem(unsigned base, unsigned size);
    void unmapmem(void* addr, unsigned size);

} /* namespace vc4cl */
#endif /* VC4CL_V3D */
