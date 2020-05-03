/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#endif

#include "icd_loader.h"

#include "Device.h"
#include "Platform.h"
#include "common.h"
#include "extensions.h"

#include <type_traits>

/*
 * Specification for the ICD loader:
 * https://www.khronos.org/registry/OpenCL/extensions/khr/cl_khr_icd.txt
 *
 * "The ICD Loader queries the following functions from the library:
 * clIcdGetPlatformIDsKHR, clGetPlatformInfo, and clGetExtensionFunctionAddress."
 */

cl_int VC4CL_FUNC(clIcdGetPlatformIDsKHR)(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms)
{
    VC4CL_PRINT_API_CALL("cl_int", clIcdGetPlatformIDsKHR, "cl_uint", num_entries, "cl_platform_id*", platforms,
        "cl_uint*", num_platforms);
    if((num_entries == 0) != (platforms == nullptr))
        return vc4cl::returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameter is empty!");

    if(num_platforms != nullptr)
    {
        *num_platforms = 1;
    }
    if(platforms != nullptr)
    {
        *platforms = vc4cl::Platform::getVC4CLPlatform().toBase();
    }
    return CL_SUCCESS;
}

#ifdef use_cl_khr_icd
// Ignore the missing initializer error here, since depending on the system, there might be different amount of entries
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/*
 * The move of the ICD dispatch table header from the Khronos ICD implementation repository to the OpenCL-Headers
 * repository also changed the contents of the ICD dispatch type.
 *
 * In the old version (e.g. as installed on the Raspberry Pi), a function which was introduced in a later OpenCL version
 * is defined as e.g.:
 *
 * #ifdef CL_VERSION_2_0
 * CL_API_ENTRY cl_command_queue (CL_API_CALL*clCreateCommandQueueWithProperties)(
 * cl_context context, cl_device_id device , const cl_queue_properties* properties, cl_int* errcode_ret)
 * CL_API_SUFFIX__VERSION_2_0;
 * #else
 * CL_API_ENTRY cl_int (CL_API_CALL* clUnknown123)(void);
 * #endif
 *
 * This defines (depending on the OpenCL version set) the members clCreateCommandQueueWithProperties or clUnknown123.
 *
 * In the new code (e.g. as used by the CI when compiling against the latest OpenCL-Headers the same function is defined
 * as e.g.:
 *
 * // Somewhere up top
 * #ifdef CL_VERSION_2_0
 * typedef CL_API_ENTRY cl_command_queue(CL_API_CALL *cl_api_clCreateCommandQueueWithProperties)(cl_context context,
 * cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret) CL_API_SUFFIX__VERSION_2_0;
 * #else
 * typedef void *cl_api_clCreateCommandQueueWithProperties;
 * #endif
 *
 * // In the _cl_icd_dispatch structure:
 * cl_api_clCreateCommandQueueWithProperties clCreateCommandQueueWithProperties;
 *
 * Thus, the member is always called clCreateCommandQueueWithProperties.
 *
 * Therefore, we need to determine which member is actually defined and cast our function to the proper member type.
 */

namespace detail
{
#define DEFINE_MEMBER_TYPE(Preferred, Fallback)                                                                        \
    template <class T>                                                                                                 \
    static auto type_##Preferred##Fallback(int)->decltype(std::declval<T>().Preferred);                                \
    template <class T>                                                                                                 \
    static auto type_##Preferred##Fallback(long)->decltype(std::declval<T>().Fallback);                                \
    template <class T>                                                                                                 \
    using member_type##Preferred##Fallback = decltype(detail::type_##Preferred##Fallback<T>(0));

    DEFINE_MEMBER_TYPE(clCreateCommandQueueWithProperties, clUnknown123)
    DEFINE_MEMBER_TYPE(clCreateSamplerWithProperties, clUnknown133)
    DEFINE_MEMBER_TYPE(clCloneKernel, clUnknown137)
    DEFINE_MEMBER_TYPE(clCreateProgramWithIL, clUnknown138)
    DEFINE_MEMBER_TYPE(clSetProgramReleaseCallback, clUnknown144)
    DEFINE_MEMBER_TYPE(clSetProgramSpecializationConstant, clUnknown145)

} // namespace detail

#define MEMBER_TYPE(Preferred, Fallback) detail::member_type##Preferred##Fallback<_cl_icd_dispatch>

_cl_icd_dispatch vc4cl_dispatch = {
    // see https://github.com/KhronosGroup/OpenCL-ICD-Loader/blob/master/icd_dispatch.h
    // for implementation, see https://github.com/pocl/pocl/blob/master/lib/CL/clGetPlatformIDs.c

    /* OpenCL 1.0 */
    &VC4CL_clGetPlatformIDs,           /* clGetPlatformIDs */
    &VC4CL_clGetPlatformInfo,          /* clGetPlatformInfo */
    &VC4CL_clGetDeviceIDs,             /* clGetDeviceIDs */
    &VC4CL_clGetDeviceInfo,            /* clGetDeviceInfo */
    &VC4CL_clCreateContext,            /* clCreateContext */
    &VC4CL_clCreateContextFromType,    /* clCreateContextFromType */
    &VC4CL_clRetainContext,            /* clRetainContext */
    &VC4CL_clReleaseContext,           /* clReleaseContext */
    &VC4CL_clGetContextInfo,           /* clGetContextInfo */
    &VC4CL_clCreateCommandQueue,       /* clCreateCommandQueue */
    &VC4CL_clRetainCommandQueue,       /* clRetainCommandQueue */
    &VC4CL_clReleaseCommandQueue,      /* clReleaseCommandQueue */
    &VC4CL_clGetCommandQueueInfo,      /* clGetCommandQueueInfo */
    &VC4CL_clSetCommandQueueProperty,  /* clSetCommandQueueProperty */
    &VC4CL_clCreateBuffer,             /* clCreateBuffer */
    &VC4CL_clCreateImage2D,            /* clCreateImage2D */
    &VC4CL_clCreateImage3D,            /* clCreateImage3D */
    &VC4CL_clRetainMemObject,          /* clRetainMemObject */
    &VC4CL_clReleaseMemObject,         /* clReleaseMemObject */
    &VC4CL_clGetSupportedImageFormats, /* clGetSupportedImageFormats */
    &VC4CL_clGetMemObjectInfo,         /* clGetMemObjectInfo */
    &VC4CL_clGetImageInfo,             /* clGetImageInfo */
    &VC4CL_clCreateSampler,            /* clCreateSampler */
    &VC4CL_clRetainSampler,            /* clRetainSampler */
    &VC4CL_clReleaseSampler,           /* clReleaseSampler */
    &VC4CL_clGetSamplerInfo,           /* clGetSamplerInfo */
    &VC4CL_clCreateProgramWithSource,  /* clCreateProgramWithSource */
    &VC4CL_clCreateProgramWithBinary,  /* clCreateProgramWithBinary */
    &VC4CL_clRetainProgram,            /* clRetainProgram */
    &VC4CL_clReleaseProgram,           /* clReleaseProgram */
    &VC4CL_clBuildProgram,             /* clBuildProgram */
    &VC4CL_clUnloadCompiler,           /* clUnloadCompiler */
    &VC4CL_clGetProgramInfo,           /* clGetProgramInfo */
    &VC4CL_clGetProgramBuildInfo,      /* clGetProgramBuildInfo */
    &VC4CL_clCreateKernel,             /* clCreateKernel */
    &VC4CL_clCreateKernelsInProgram,   /* clCreateKernelsInProgram */
    &VC4CL_clRetainKernel,             /* clRetainKernel */
    &VC4CL_clReleaseKernel,            /* clReleaseKernel */
    &VC4CL_clSetKernelArg,             /* clSetKernelArg */
    &VC4CL_clGetKernelInfo,            /* clGetKernelInfo */
    &VC4CL_clGetKernelWorkGroupInfo,   /* clGetKernelWorkGroupInfo */
    &VC4CL_clWaitForEvents,            /* clWaitForEvents */
    &VC4CL_clGetEventInfo,             /* clGetEventInfo */
    &VC4CL_clRetainEvent,              /* clRetainEvent */
    &VC4CL_clReleaseEvent,             /* clReleaseEvent */
    &VC4CL_clGetEventProfilingInfo,    /* clGetEventProfilingInfo */
    &VC4CL_clFlush,                    /* clFlush */
    &VC4CL_clFinish,                   /* clFinish */
    &VC4CL_clEnqueueReadBuffer,        /* clEnqueueReadBuffer */
    &VC4CL_clEnqueueWriteBuffer,       /* clEnqueueWriteBuffer */
    &VC4CL_clEnqueueCopyBuffer,        /* clEnqueueCopyBuffer */
    &VC4CL_clEnqueueReadImage,         /* clEnqueueReadImage */
    &VC4CL_clEnqueueWriteImage,        /* clEnqueueWriteImage */
    &VC4CL_clEnqueueCopyImage,         /* clEnqueueCopyImage */
    &VC4CL_clEnqueueCopyImageToBuffer, /* clEnqueueCopyImageToBuffer */
    &VC4CL_clEnqueueCopyBufferToImage, /* clEnqueueCopyBufferToImage */
    &VC4CL_clEnqueueMapBuffer,         /* clEnqueueMapBuffer */
    &VC4CL_clEnqueueMapImage,          /* clEnqueueMapImage */
    &VC4CL_clEnqueueUnmapMemObject,    /* clEnqueueUnmapMemObject */
    &VC4CL_clEnqueueNDRangeKernel,     /* clEnqueueNDRangeKernel */
    &VC4CL_clEnqueueTask,              /* clEnqueueTask */
    &VC4CL_clEnqueueNativeKernel,      /* clEnqueueNativeKernel */
    &VC4CL_clEnqueueMarker,            /* clEnqueueMarker */
    &VC4CL_clEnqueueWaitForEvents,     /* clEnqueueWaitForEvents */
    &VC4CL_clEnqueueBarrier,           /* clEnqueueBarrier */
    &clGetExtensionFunctionAddress,    /* clGetExtensionFunctionAddress */

    /* cl_khr_gl_sharing */
    nullptr, /* clCreateFromGLBuffer */
    nullptr, /* clCreateFromGLTexture2D */
    nullptr, /* clCreateFromGLTexture3D */
    nullptr, /* clCreateFromGLRenderbuffer */
    nullptr, /* clGetGLObjectInfo */
    nullptr, /* clGetGLTextureInfo */
    nullptr, /* clEnqueueAcquireGLObjects */
    nullptr, /* clEnqueueReleaseGLObjects */
    nullptr, /* clGetGLContextInfoKHR */

    /* cl_khr_d3d10_sharing */
    nullptr, /* clGetDeviceIDsFromD3D10KHR */
    nullptr, /* clCreateFromD3D10BufferKHR */
    nullptr, /* clCreateFromD3D10Texture2DKHR */
    nullptr, /* clCreateFromD3D10Texture3DKHR */
    nullptr, /* clEnqueueAcquireD3D10ObjectsKHR */
    nullptr, /* clEnqueueReleaseD3D10ObjectsKHR */

    /* OpenCL 1.1 */
    &VC4CL_clSetEventCallback,               /* clSetEventCallback */
    &VC4CL_clCreateSubBuffer,                /* clCreateSubBuffer */
    &VC4CL_clSetMemObjectDestructorCallback, /* clSetMemObjectDestructorCallback */
    &VC4CL_clCreateUserEvent,                /* clCreateUserEvent */
    &VC4CL_clSetUserEventStatus,             /* clSetUserEventStatus */
    &VC4CL_clEnqueueReadBufferRect,          /* clEnqueueReadBufferRect */
    &VC4CL_clEnqueueWriteBufferRect,         /* clEnqueueWriteBufferRect */
    &VC4CL_clEnqueueCopyBufferRect,          /* clEnqueueCopyBufferRect */

    /* cl_ext_device_fission */
    nullptr, /* clCreateSubDevicesEXT */
    nullptr, /* clRetainDeviceEXT */
    nullptr, /* clReleaseDeviceEXT */

    /* cl_khr_gl_event */
    nullptr, /* clCreateEventFromGLsyncKHR */

    /* OpenCL 1.2 */
    &VC4CL_clCreateSubDevices,                       /* clCreateSubDevices */
    &VC4CL_clRetainDevice,                           /* clRetainDevice */
    &VC4CL_clReleaseDevice,                          /* clReleaseDevice */
    &VC4CL_clCreateImage,                            /* clCreateImage */
    &VC4CL_clCreateProgramWithBuiltInKernels,        /* clCreateProgramWithBuiltInKernels */
    &VC4CL_clCompileProgram,                         /* clCompileProgram */
    &VC4CL_clLinkProgram,                            /* clLinkProgram */
    &VC4CL_clUnloadPlatformCompiler,                 /* clUnloadPlatformCompiler */
    &VC4CL_clGetKernelArgInfo,                       /* clGetKernelArgInfo */
    &VC4CL_clEnqueueFillBuffer,                      /* clEnqueueFillBuffer */
    &VC4CL_clEnqueueFillImage,                       /* clEnqueueFillImage */
    &VC4CL_clEnqueueMigrateMemObjects,               /* clEnqueueMigrateMemObjects */
    &VC4CL_clEnqueueMarkerWithWaitList,              /* clEnqueueMarkerWithWaitList */
    &VC4CL_clEnqueueBarrierWithWaitList,             /* clEnqueueBarrierWithWaitList */
    &VC4CL_clGetExtensionFunctionAddressForPlatform, /* clGetExtensionFunctionAddressForPlatform */

    /* cl_khr_gl_sharing */
    nullptr, /* clCreateFromGLTexture */

    /* cl_khr_d3d11_sharing */
    nullptr, /* clGetDeviceIDsFromD3D11KHR */
    nullptr, /* clCreateFromD3D11BufferKHR */
    nullptr, /* clCreateFromD3D11Texture2DKHR */
    nullptr, /* clCreateFromD3D11Texture3DKHR */
    nullptr, /* clCreateFromDX9MediaSurfaceKHR */
    nullptr, /* clEnqueueAcquireD3D11ObjectsKHR */
    nullptr, /* clEnqueueReleaseD3D11ObjectsKHR */

    /* cl_khr_dx9_media_sharing */
    nullptr, /* clGetDeviceIDsFromDX9MediaAdapterKHR */
    nullptr, /* clEnqueueAcquireDX9MediaSurfacesKHR */
    nullptr, /* clEnqueueReleaseDX9MediaSurfacesKHR */

    /* cl_khr_egl_image */
    nullptr, /* clCreateFromEGLImageKHR */
    nullptr, /* clEnqueueAcquireEGLObjectsKHR */
    nullptr, /* clEnqueueReleaseEGLObjectsKHR */

    /* cl_khr_egl_event */
    nullptr, /* clCreateEventFromEGLSyncKHR */

    /* OpenCL 2.0 */
    reinterpret_cast<MEMBER_TYPE(clCreateCommandQueueWithProperties, clUnknown123)>(
        &VC4CL_clCreateCommandQueueWithPropertiesKHR), /* clCreateCommandQueueWithProperties */
#ifdef CL_VERSION_2_0
    &VC4CL_clCreatePipe,                  /* clCreatePipe */
    &VC4CL_clGetPipeInfo,                 /* clGetPipeInfo */
    &VC4CL_clSVMAlloc,                    /* clSVMAlloc */
    &VC4CL_clSVMFree,                     /* clSVMFree */
    &VC4CL_clEnqueueSVMFree,              /* clEnqueueSVMFree */
    &VC4CL_clEnqueueSVMMemcpy,            /* clEnqueueSVMMemcpy */
    &VC4CL_clEnqueueSVMMemFill,           /* clEnqueueSVMMemFill */
    &VC4CL_clEnqueueSVMMap,               /* clEnqueueSVMMap */
    &VC4CL_clEnqueueSVMUnmap,             /* clEnqueueSVMUnmap */
    &VC4CL_clCreateSamplerWithProperties, /* clCreateSamplerWithProperties */
    &VC4CL_clSetKernelArgSVMPointer,      /* clSetKernelArgSVMPointer */
    &VC4CL_clSetKernelExecInfo,           /* clSetKernelExecInfo */
#else
    nullptr, /* clCreatePipe */
    nullptr, /* clGetPipeInfo */
    nullptr, /* clSVMAlloc */
    nullptr, /* clSVMFree */
    nullptr, /* clEnqueueSVMFree */
    nullptr, /* clEnqueueSVMMemcpy */
    nullptr, /* clEnqueueSVMMemFill */
    nullptr, /* clEnqueueSVMMap */
    nullptr, /* clEnqueueSVMUnmap */
    nullptr, /* clCreateSamplerWithProperties */
    nullptr, /* clSetKernelArgSVMPointer */
    nullptr, /* clSetKernelExecInfo */
#endif

/* cl_khr_sub_groups */
#ifdef CL_VERSION_2_1
    &VC4CL_clGetKernelSubGroupInfo, /* clGetKernelSubGroupInfoKHR */
#else
    nullptr, /* clGetKernelSubGroupInfoKHR */
#endif

    /* OpenCL 2.1 */
    reinterpret_cast<MEMBER_TYPE(clCloneKernel, clUnknown137)>(&VC4CL_clCloneKernel), /* clCloneKernel */
    reinterpret_cast<MEMBER_TYPE(clCreateProgramWithIL, clUnknown138)>(
        &VC4CL_clCreateProgramWithILKHR), /* clCreateProgramWithIL */

#ifdef CL_VERSION_2_1
    &VC4CL_clEnqueueSVMMigrateMem,         /* clEnqueueSVMMigrateMem */
    &VC4CL_clGetDeviceAndHostTimer,        /* clGetDeviceAndHostTimer */
    &VC4CL_clGetHostTimer,                 /* clGetHostTimer */
    &VC4CL_clGetKernelSubGroupInfo,        /* clGetKernelSubGroupInfo */
    &VC4CL_clSetDefaultDeviceCommandQueue, /* clSetDefaultDeviceCommandQueue */
#else
    nullptr, /* clEnqueueSVMMigrateMem */
    nullptr, /* clGetDeviceAndHostTimer */
    nullptr, /* clGetHostTimer */
    nullptr, /* clGetKernelSubGroupInfo */
    nullptr, /* clSetDefaultDeviceCommandQueue */
#endif

    /* OpenCL 2.2 */
    reinterpret_cast<MEMBER_TYPE(clSetProgramReleaseCallback, clUnknown144)>(
        &VC4CL_clSetProgramReleaseCallback), /* clSetProgramReleaseCallback */
    reinterpret_cast<MEMBER_TYPE(clSetProgramSpecializationConstant, clUnknown145)>(
        &VC4CL_clSetProgramSpecializationConstant), /* clSetProgramSpecializationConstant */

/* OpenCL 3.0 */
#ifdef CL_VERSION_3_0
    &VC4CL_clCreateBufferWithProperties, /* clCreateBufferWithProperties */
    &VC4CL_clCreateImageWithProperties   /* clCreateImageWithProperties */
#else
    nullptr, /* clCreateBufferWithProperties */
    nullptr  /* clCreateImageWithProperties */
#endif
};
#pragma GCC diagnostic pop
#endif
