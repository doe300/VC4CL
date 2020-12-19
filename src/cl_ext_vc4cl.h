/*
 * Public definitions for any VC4CL OpenCL extension.
 *
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#pragma once

#include <CL/opencl.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /*
     * VC4CL performance counters (cl_vc4cl_performance_counters)
     */

    typedef cl_profiling_info cl_performance_counter_vc4cl;

#define CL_PROFILING_PERFORMANCE_COUNTER_EXECUTION_CYCLES_VC4CL (CL_PROFILING_COMMAND_END + 10)
#define CL_PROFILING_PERFORMANCE_COUNTER_IDLE_CYCLES_VC4CL (CL_PROFILING_COMMAND_END + 11)
#define CL_PROFILING_PERFORMANCE_COUNTER_INSTRUCTION_COUNT_VC4CL (CL_PROFILING_COMMAND_END + 12)
#define CL_PROFILING_PERFORMANCE_COUNTER_INSTRUCTION_CACHE_MISSES_VC4CL (CL_PROFILING_COMMAND_END + 13)
#define CL_PROFILING_PERFORMANCE_COUNTER_L2_CACHE_MISSES_VC4CL (CL_PROFILING_COMMAND_END + 14)

#ifdef __cplusplus
}
#endif
