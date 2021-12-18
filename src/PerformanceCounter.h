/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef PERFORMANCE_COUNTER_H
#define PERFORMANCE_COUNTER_H

#include "cl_ext_vc4cl.h"

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <chrono>

namespace vc4cl
{
    struct KernelHeader;
    enum class CounterType : unsigned char;

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
        size_t numQPUs;
        std::chrono::microseconds elapsedTime;

        void dumpCounters() const;

        cl_int getCounterValue(cl_profiling_info param_name, size_t param_value_size, void* param_value,
            size_t* param_value_size_ret) const;
    };

    /**
     * RAII wrapper for configuring and collecting performance counter values
     */
    class PerformanceCollector
    {
    public:
        PerformanceCollector(
            PerformanceCounters& counters, const KernelHeader& kernel, size_t numQPUs, size_t numGroups);
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
