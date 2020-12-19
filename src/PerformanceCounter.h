/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef PERFORMANCE_COUNTER_H
#define PERFORMANCE_COUNTER_H

#include "V3D.h"
#include "cl_ext_vc4cl.h"

#include <map>
#include <mutex>
#include <set>
#include <string>

namespace vc4cl
{
    struct KernelInfo;

    /**
     * Container object for storing performance counter results
     */
    struct PerformanceCounters
    {
        mutable std::mutex countersLock;
        std::map<CounterType, int64_t> counterValues;
        bool querySuccessful = true;
        size_t clockSpeed;
        size_t numInstructions;
        size_t numExplicitUniforms;
        size_t numWorkGroups;
        size_t workGroupSize;

        void dumpCounters() const;
    };

    /**
     * RAII wrapper for configuring and collecting performance counter values
     */
    class PerformanceCollector
    {
    public:
        PerformanceCollector(
            PerformanceCounters& counters, const KernelInfo& kernelInfo, size_t localWorkSize, size_t numGroups);
        PerformanceCollector(const PerformanceCollector&) = delete;
        PerformanceCollector(PerformanceCollector&&) noexcept = delete;
        ~PerformanceCollector() noexcept;

        PerformanceCollector& operator=(const PerformanceCollector&) = delete;
        PerformanceCollector& operator=(PerformanceCollector&&) noexcept = delete;

    private:
        PerformanceCounters& counters;
    };
} /* namespace vc4cl */

#endif /* PERFORMANCE_COUNTER_H */
