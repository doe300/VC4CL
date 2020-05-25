/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "extensions.h"

#include "Device.h"
#include "Platform.h"
#include "icd_loader.h"

#include <CL/opencl.h>
#include <cstdio>
#include <cstring>

using namespace vc4cl;

void* VC4CL_FUNC(clGetExtensionFunctionAddressForPlatform)(cl_platform_id platform, const char* funcname)
{
    VC4CL_PRINT_API_CALL(
        "void*", clGetExtensionFunctionAddressForPlatform, "cl_platform_id", platform, "const char*", funcname);
    DEBUG_LOG(DebugLevel::API_CALLS, std::cout << "get extension function address: " << funcname << std::endl)
    // the clIcdGetPlatformIDsKHR function is looked up via here
    if(strcmp("clIcdGetPlatformIDsKHR", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clIcdGetPlatformIDsKHR));
    if(strcmp("clGetPlatformInfo", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clGetPlatformInfo));

    // cl_khr_il_program
    if(strcmp("clCreateProgramWithILKHR", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clCreateProgramWithILKHR)));

    // cl_khr_create_command_queue
    if(strcmp("clCreateCommandQueueWithPropertiesKHR", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clCreateCommandQueueWithPropertiesKHR));

    // cl_altera_live_object_tracking
    if(strcmp("clTrackLiveObjectsAltera", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clTrackLiveObjectsAltera)));
    if(strcmp("clReportLiveObjectsAltera", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clReportLiveObjectsAltera)));

    // cl_vc4cl_performance_counters
    if(strcmp("clCreatePerformanceCounterVC4CL", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clCreatePerformanceCounterVC4CL));
    if(strcmp("clGetPerformanceCounterValueVC4CL", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clGetPerformanceCounterValueVC4CL));
    if(strcmp("clReleasePerformanceCounterVC4CL", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clReleasePerformanceCounterVC4CL));
    if(strcmp("clRetainPerformanceCounterVC4CL", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clRetainPerformanceCounterVC4CL));
    if(strcmp("clResetPerformanceCounterVC4CL", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clResetPerformanceCounterValueVC4CL));

    DEBUG_LOG(DebugLevel::API_CALLS, std::cout << "extension function address not found for: " << funcname << std::endl)

    return nullptr;
}

void* clGetExtensionFunctionAddress(const char* name)
{
    return VC4CL_FUNC(clGetExtensionFunctionAddressForPlatform)(Platform::getVC4CLPlatform().toBase(), name);
}
