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
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] get extension function address: " << funcname << std::endl;
#endif
    // the clIcdGetPlatformIDsKHR function is looked up via here
    if(strcmp("clIcdGetPlatformIDsKHR", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clIcdGetPlatformIDsKHR));
    if(strcmp("clGetPlatformInfo", funcname) == 0)
        return reinterpret_cast<void*>(&VC4CL_FUNC(clGetPlatformInfo));

    // cl_khr_il_program
    if(strcmp("clCreateProgramWithILKHR", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clCreateProgramWithILKHR)));

    // cl_arm_shared_virtual_memory
    if(strcmp("clSVMAllocARM", funcname) == 0)
        return reinterpret_cast<void*>((&VC4CL_FUNC(clSVMAllocARM)));
    if(strcmp("clSVMFreeARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clSVMFreeARM)));
    if(strcmp("clEnqueueSVMFreeARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clEnqueueSVMFreeARM)));
    if(strcmp("clEnqueueSVMMemcpyARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clEnqueueSVMMemcpyARM)));
    if(strcmp("clEnqueueSVMMemFillARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clEnqueueSVMMemFillARM)));
    if(strcmp("clEnqueueSVMMapARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clEnqueueSVMMapARM)));
    if(strcmp("clEnqueueSVMUnmapARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clEnqueueSVMUnmapARM)));
    if(strcmp("clSetKernelArgSVMPointerARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clSetKernelArgSVMPointerARM)));
    if(strcmp("clSetKernelExecInfoARM", funcname) == 0)
        return reinterpret_cast<void*>(&(VC4CL_FUNC(clSetKernelExecInfoARM)));

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

#ifdef DEBUG_MODE
    std::cout << "[VC4CL] extension function address not found for: " << funcname << std::endl;
#endif

    return nullptr;
}

void* clGetExtensionFunctionAddress(const char* name)
{
    return VC4CL_FUNC(clGetExtensionFunctionAddressForPlatform)(Platform::getVC4CLPlatform().toBase(), name);
}
