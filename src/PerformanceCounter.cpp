/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "PerformanceCounter.h"

#include "Program.h"
#include "hal/V3D.h"
#include "hal/hal.h"

#include <chrono>
#include <cmath>
#include <vector>

using namespace vc4cl;

static const std::vector<std::pair<CounterType, std::string>> PERFORMANCE_COUNTERS = {
    {CounterType::EXECUTION_CYCLES, "Execution cycles"},
    {CounterType::IDLE_CYCLES, "Idle cycles"},
    {CounterType::INSTRUCTION_CACHE_HITS, "Instruction cache lookups"},
    {CounterType::INSTRUCTION_CACHE_MISSES, "Instruction cache misses"},
    {CounterType::L2_CACHE_HITS, "L2 cache hits"},
    {CounterType::L2_CACHE_MISSES, "L2 cache misses"},
    {CounterType::TMU_CACHE_MISSES, "TMU cache misses"},
    {CounterType::TMU_STALL_CYCLES, "TMU stall cycles"},
    {CounterType::TMU_TOTAL_WORDS, "TMU words loaded"},
    {CounterType::UNIFORM_CACHE_HITS, "Uniform cache lookups"},
    {CounterType::UNIFORM_CACHE_MISSES, "Uniform cache misses"},
    {CounterType::VCD_STALL_CYCLES, "VPM DMA read stall cycles"},
    {CounterType::VDW_STALL_CYCES, "VPM DMA write stall cycles"},
};

void PerformanceCounters::dumpCounters() const
{
    std::lock_guard<std::mutex> guard(countersLock);
    if(!querySuccessful)
    {
        DEBUG_LOG(DebugLevel::PERFORMANCE_COUNTERS,
            std::cout << "Failed to query performance counters, no results available!" << std::endl)
        return;
    }
    DEBUG_LOG(
        DebugLevel::PERFORMANCE_COUNTERS, std::cout << "Elapsed time: " << elapsedTime.count() << "us" << std::endl)
    DEBUG_LOG(DebugLevel::PERFORMANCE_COUNTERS, std::cout << "Clock speed: " << clockSpeed << std::endl)
    DEBUG_LOG(DebugLevel::PERFORMANCE_COUNTERS, std::cout << "Instruction count: " << numInstructions << std::endl)
    DEBUG_LOG(
        DebugLevel::PERFORMANCE_COUNTERS, std::cout << "Explicit uniform count: " << numExplicitUniforms << std::endl)
    DEBUG_LOG(DebugLevel::PERFORMANCE_COUNTERS, std::cout << "QPUs used: " << numQPUs << std::endl)
    DEBUG_LOG(DebugLevel::PERFORMANCE_COUNTERS, std::cout << "Kernel repetition count: " << numWorkGroups << std::endl)
    for(const auto& counter : PERFORMANCE_COUNTERS)
    {
        DEBUG_LOG(DebugLevel::PERFORMANCE_COUNTERS,
            std::cout << counter.second << ": " << counterValues.at(counter.first) << std::endl)
    }
}

cl_int PerformanceCounters::getCounterValue(
    cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) const
{
    switch(param_name)
    {
    case CL_PROFILING_PERFORMANCE_COUNTER_EXECUTION_CYCLES_VC4CL:
    {
        auto counterIt = counterValues.find(CounterType::EXECUTION_CYCLES);
        if(counterIt == counterValues.end() || counterIt->second < 0)
            break;
        return returnValue(
            static_cast<cl_ulong>(counterIt->second), param_value_size, param_value, param_value_size_ret);
    }
    case CL_PROFILING_PERFORMANCE_COUNTER_IDLE_CYCLES_VC4CL:
    {
        auto counterIt = counterValues.find(CounterType::IDLE_CYCLES);
        if(counterIt == counterValues.end() || counterIt->second < 0)
            break;
        return returnValue(
            static_cast<cl_ulong>(counterIt->second), param_value_size, param_value, param_value_size_ret);
    }
    case CL_PROFILING_PERFORMANCE_COUNTER_INSTRUCTION_CACHE_MISSES_VC4CL:
    {
        auto counterIt = counterValues.find(CounterType::INSTRUCTION_CACHE_MISSES);
        if(counterIt == counterValues.end() || counterIt->second < 0)
            break;
        return returnValue(
            static_cast<cl_ulong>(counterIt->second), param_value_size, param_value, param_value_size_ret);
    }
    case CL_PROFILING_PERFORMANCE_COUNTER_L2_CACHE_MISSES_VC4CL:
    {
        auto counterIt = counterValues.find(CounterType::L2_CACHE_MISSES);
        if(counterIt == counterValues.end() || counterIt->second < 0)
            break;
        return returnValue(
            static_cast<cl_ulong>(counterIt->second), param_value_size, param_value, param_value_size_ret);
    }
    case CL_PROFILING_PERFORMANCE_COUNTER_INSTRUCTION_COUNT_VC4CL:
        return returnValue(static_cast<cl_ulong>(numInstructions), param_value_size, param_value, param_value_size_ret);
    }

    return returnError(
        CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_performance_counter_vc4cl value %d", param_name));
}

PerformanceCollector::PerformanceCollector(
    PerformanceCounters& counters, const KernelHeader& kernel, size_t numQPUs, size_t numGroups) :
    counters(counters)
{
    // set-up and clear the performance counters
    std::lock_guard<std::mutex> guard(counters.countersLock);
    auto sys = system();
    auto v3d = system()->getV3DIfAvailable();
    if(!v3d || !sys)
    {
        counters.querySuccessful = false;
        DEBUG_LOG(DebugLevel::PERFORMANCE_COUNTERS,
            std::cout << "Performance counters are disabled, since V3D interface is not active!" << std::endl)
        return;
    }
    // fill the static kernel info
    counters.clockSpeed = sys->getCurrentQPUClockRateInHz();
    counters.numInstructions = kernel.getLength();
    counters.numExplicitUniforms = static_cast<uint32_t>(kernel.getExplicitUniformCount());
    counters.numWorkGroups = numGroups;
    counters.numQPUs = numQPUs;
    for(uint8_t i = 0; i < PERFORMANCE_COUNTERS.size(); ++i)
    {
        if(!v3d->setCounter(i, PERFORMANCE_COUNTERS[i].first))
        {
            counters.querySuccessful = false;
            break;
        }
    }
    // set to start timestamp
    counters.elapsedTime = std::chrono::duration_cast<decltype(counters.elapsedTime)>(
        std::chrono::high_resolution_clock::now().time_since_epoch());
}

PerformanceCollector::~PerformanceCollector() noexcept
{
    // read and unset the performance counters
    if(!counters.querySuccessful)
        return;
    std::lock_guard<std::mutex> guard(counters.countersLock);
    // calculate actual duration
    counters.elapsedTime = std::chrono::duration_cast<decltype(counters.elapsedTime)>(
        std::chrono::high_resolution_clock::now().time_since_epoch() - counters.elapsedTime);
    auto v3d = system()->getV3DIfAvailable();
    if(!v3d)
    {
        counters.querySuccessful = false;
        return;
    }
    for(uint8_t i = 0; i < PERFORMANCE_COUNTERS.size(); ++i)
    {
        auto val = v3d->getCounter(i);
        v3d->disableCounter(i);
        if(val == -1)
        {
            counters.querySuccessful = false;
            break;
        }
        counters.counterValues[PERFORMANCE_COUNTERS[i].first] += val;
    }
}
