/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "icd_loader.h"

#include "common.h"
#include "extensions.h"
#include "Device.h"
#include "Platform.h"

#include <CL/opencl.h>

/*
 * Specification for the ICD loader:
 * https://www.khronos.org/registry/OpenCL/extensions/khr/cl_khr_icd.txt
 * 
 * "The ICD Loader queries the following functions from the library:
 * clIcdGetPlatformIDsKHR, clGetPlatformInfo, and clGetExtensionFunctionAddress."
 */

cl_int VC4CL_FUNC(clIcdGetPlatformIDsKHR)(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms)
{
	if ((num_entries == 0) != (platforms == nullptr))
		return vc4cl::returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameter is empty!");

	if (num_platforms != nullptr) {
		*num_platforms = 1;
	}
	if (platforms != nullptr) {
		*platforms = vc4cl::Platform::getVC4CLPlatform().toBase();
	}
	return CL_SUCCESS;
}

#ifdef use_cl_khr_icd

struct _cl_icd_dispatch
{
	void* buffer[200];
};

struct _cl_icd_dispatch vc4cl_dispatch = {
	//see https://github.com/KhronosGroup/OpenCL-ICD-Loader/blob/master/icd_dispatch.h
	//for implementation, see https://github.com/pocl/pocl/blob/master/lib/CL/clGetPlatformIDs.c

	/* OpenCL 1.0 */
	reinterpret_cast<void*>(&VC4CL_clGetPlatformIDs), /* clGetPlatformIDs */
	reinterpret_cast<void*>(&VC4CL_clGetPlatformInfo), /* clGetPlatformInfo */
	reinterpret_cast<void*>(&VC4CL_clGetDeviceIDs), /* clGetDeviceIDs */
	reinterpret_cast<void*>(&VC4CL_clGetDeviceInfo), /* clGetDeviceInfo */
	reinterpret_cast<void*>(&VC4CL_clCreateContext), /* clCreateContext */
	reinterpret_cast<void*>(&VC4CL_clCreateContextFromType), /* clCreateContextFromType */
	reinterpret_cast<void*>(&VC4CL_clRetainContext), /* clRetainContext */
	reinterpret_cast<void*>(&VC4CL_clReleaseContext), /* clReleaseContext */
	reinterpret_cast<void*>(&VC4CL_clGetContextInfo), /* clGetContextInfo */
	reinterpret_cast<void*>(&VC4CL_clCreateCommandQueue), /* clCreateCommandQueue */
	reinterpret_cast<void*>(&VC4CL_clRetainCommandQueue), /* clRetainCommandQueue */
	reinterpret_cast<void*>(&VC4CL_clReleaseCommandQueue), /* clReleaseCommandQueue */
	reinterpret_cast<void*>(&VC4CL_clGetCommandQueueInfo), /* clGetCommandQueueInfo */
	reinterpret_cast<void*>(&VC4CL_clSetCommandQueueProperty), /* clSetCommandQueueProperty */
	reinterpret_cast<void*>(&VC4CL_clCreateBuffer), /* clCreateBuffer */
	reinterpret_cast<void*>(&VC4CL_clCreateImage2D), /* clCreateImage2D */
	reinterpret_cast<void*>(&VC4CL_clCreateImage3D), /* clCreateImage3D */
	reinterpret_cast<void*>(&VC4CL_clRetainMemObject), /* clRetainMemObject */
	reinterpret_cast<void*>(&VC4CL_clReleaseMemObject), /* clReleaseMemObject */
	reinterpret_cast<void*>(&VC4CL_clGetSupportedImageFormats), /* clGetSupportedImageFormats */
	reinterpret_cast<void*>(&VC4CL_clGetMemObjectInfo), /* clGetMemObjectInfo */
	reinterpret_cast<void*>(&VC4CL_clGetImageInfo), /* clGetImageInfo */
	reinterpret_cast<void*>(&VC4CL_clCreateSampler), /* clCreateSampler */
	reinterpret_cast<void*>(&VC4CL_clRetainSampler), /* clRetainSampler */
	reinterpret_cast<void*>(&VC4CL_clReleaseSampler), /* clReleaseSampler */
	reinterpret_cast<void*>(&VC4CL_clGetSamplerInfo), /* clGetSamplerInfo */
	reinterpret_cast<void*>(&VC4CL_clCreateProgramWithSource), /* clCreateProgramWithSource */
	reinterpret_cast<void*>(&VC4CL_clCreateProgramWithBinary), /* clCreateProgramWithBinary */
	reinterpret_cast<void*>(&VC4CL_clRetainProgram), /* clRetainProgram */
	reinterpret_cast<void*>(&VC4CL_clReleaseProgram), /* clReleaseProgram */
	reinterpret_cast<void*>(&VC4CL_clBuildProgram), /* clBuildProgram */
	reinterpret_cast<void*>(&VC4CL_clUnloadCompiler), /* clUnloadCompiler */
	reinterpret_cast<void*>(&VC4CL_clGetProgramInfo), /* clGetProgramInfo */
	reinterpret_cast<void*>(&VC4CL_clGetProgramBuildInfo), /* clGetProgramBuildInfo */
	reinterpret_cast<void*>(&VC4CL_clCreateKernel), /* clCreateKernel */
	reinterpret_cast<void*>(&VC4CL_clCreateKernelsInProgram), /* clCreateKernelsInProgram */
	reinterpret_cast<void*>(&VC4CL_clRetainKernel), /* clRetainKernel */
	reinterpret_cast<void*>(&VC4CL_clReleaseKernel), /* clReleaseKernel */
	reinterpret_cast<void*>(&VC4CL_clSetKernelArg), /* clSetKernelArg */
	reinterpret_cast<void*>(&VC4CL_clGetKernelInfo), /* clGetKernelInfo */
	reinterpret_cast<void*>(&VC4CL_clGetKernelWorkGroupInfo), /* clGetKernelWorkGroupInfo */
	reinterpret_cast<void*>(&VC4CL_clWaitForEvents), /* clWaitForEvents */
	reinterpret_cast<void*>(&VC4CL_clGetEventInfo), /* clGetEventInfo */
	reinterpret_cast<void*>(&VC4CL_clRetainEvent), /* clRetainEvent */
	reinterpret_cast<void*>(&VC4CL_clReleaseEvent), /* clReleaseEvent */
	reinterpret_cast<void*>(&VC4CL_clGetEventProfilingInfo), /* clGetEventProfilingInfo */
	reinterpret_cast<void*>(	&VC4CL_clFlush), /* clFlush */
	reinterpret_cast<void*>(&VC4CL_clFinish), /* clFinish */
	reinterpret_cast<void*>(&VC4CL_clEnqueueReadBuffer), /* clEnqueueReadBuffer */
	reinterpret_cast<void*>(&VC4CL_clEnqueueWriteBuffer), /* clEnqueueWriteBuffer */
	reinterpret_cast<void*>(&VC4CL_clEnqueueCopyBuffer), /* clEnqueueCopyBuffer */
	reinterpret_cast<void*>(&VC4CL_clEnqueueReadImage), /* clEnqueueReadImage */
	reinterpret_cast<void*>(&VC4CL_clEnqueueWriteImage), /* clEnqueueWriteImage */
	reinterpret_cast<void*>(&VC4CL_clEnqueueCopyImage), /* clEnqueueCopyImage */
	reinterpret_cast<void*>(&VC4CL_clEnqueueCopyImageToBuffer), /* clEnqueueCopyImageToBuffer */
	reinterpret_cast<void*>(&VC4CL_clEnqueueCopyBufferToImage), /* clEnqueueCopyBufferToImage */
	reinterpret_cast<void*>(&VC4CL_clEnqueueMapBuffer), /* clEnqueueMapBuffer */
	reinterpret_cast<void*>(&VC4CL_clEnqueueMapImage), /* clEnqueueMapImage */
	reinterpret_cast<void*>(&VC4CL_clEnqueueUnmapMemObject), /* clEnqueueUnmapMemObject */
	reinterpret_cast<void*>(&VC4CL_clEnqueueNDRangeKernel), /* clEnqueueNDRangeKernel */
	reinterpret_cast<void*>(&VC4CL_clEnqueueTask), /* clEnqueueTask */
	reinterpret_cast<void*>(&VC4CL_clEnqueueNativeKernel), /* clEnqueueNativeKernel */
	reinterpret_cast<void*>(&VC4CL_clEnqueueMarker), /* clEnqueueMarker */
	reinterpret_cast<void*>(&VC4CL_clEnqueueWaitForEvents), /* clEnqueueWaitForEvents */
	reinterpret_cast<void*>(&VC4CL_clEnqueueBarrier), /* clEnqueueBarrier */
	reinterpret_cast<void*>(&clGetExtensionFunctionAddress), /* clGetExtensionFunctionAddress */
	
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
	reinterpret_cast<void*>(&VC4CL_clSetEventCallback), /* clSetEventCallback */
	reinterpret_cast<void*>(&VC4CL_clCreateSubBuffer), /* clCreateSubBuffer */
	reinterpret_cast<void*>(&VC4CL_clSetMemObjectDestructorCallback), /* clSetMemObjectDestructorCallback */
	reinterpret_cast<void*>(&VC4CL_clCreateUserEvent), /* clCreateUserEvent */
	reinterpret_cast<void*>(&VC4CL_clSetUserEventStatus), /* clSetUserEventStatus */
	reinterpret_cast<void*>(&VC4CL_clEnqueueReadBufferRect), /* clEnqueueReadBufferRect */
	reinterpret_cast<void*>(&VC4CL_clEnqueueWriteBufferRect), /* clEnqueueWriteBufferRect */
	reinterpret_cast<void*>(&VC4CL_clEnqueueCopyBufferRect), /* clEnqueueCopyBufferRect */

	/* cl_ext_device_fission */
	nullptr, /* clCreateSubDevicesEXT */
	nullptr, /* clRetainDeviceEXT */
	nullptr, /* clReleaseDeviceEXT */

	/* cl_khr_gl_event */
	nullptr, /* clCreateEventFromGLsyncKHR */

	/* OpenCL 1.2 */
	reinterpret_cast<void*>(&VC4CL_clCreateSubDevices), /* clCreateSubDevices */
	reinterpret_cast<void*>(&VC4CL_clRetainDevice), /* clRetainDevice */
	reinterpret_cast<void*>(&VC4CL_clReleaseDevice), /* clReleaseDevice */
	reinterpret_cast<void*>(&VC4CL_clCreateImage), /* clCreateImage */
	reinterpret_cast<void*>(&VC4CL_clCreateProgramWithBuiltInKernels), /* clCreateProgramWithBuiltInKernels */
	reinterpret_cast<void*>(&VC4CL_clCompileProgram), /* clCompileProgram */
	reinterpret_cast<void*>(&VC4CL_clLinkProgram), /* clLinkProgram */
	reinterpret_cast<void*>(&VC4CL_clUnloadPlatformCompiler), /* clUnloadPlatformCompiler */
	reinterpret_cast<void*>(&VC4CL_clGetKernelArgInfo), /* clGetKernelArgInfo */
	reinterpret_cast<void*>(&VC4CL_clEnqueueFillBuffer), /* clEnqueueFillBuffer */
	reinterpret_cast<void*>(&VC4CL_clEnqueueFillImage), /* clEnqueueFillImage */
	reinterpret_cast<void*>(&VC4CL_clEnqueueMigrateMemObjects), /* clEnqueueMigrateMemObjects */
	reinterpret_cast<void*>(&VC4CL_clEnqueueMarkerWithWaitList), /* clEnqueueMarkerWithWaitList */
	reinterpret_cast<void*>(&VC4CL_clEnqueueBarrierWithWaitList), /* clEnqueueBarrierWithWaitList */
	reinterpret_cast<void*>(&VC4CL_clGetExtensionFunctionAddressForPlatform), /* clGetExtensionFunctionAddressForPlatform */
	
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
	nullptr, /* clCreateCommandQueueWithProperties */
	nullptr, /* clCreatePipe */
	nullptr, /* clGetPipeInfo */
	reinterpret_cast<void*>(&VC4CL_clSVMAllocARM), /* clSVMAlloc */
	reinterpret_cast<void*>(&VC4CL_clSVMFreeARM), /* clSVMFree */
	reinterpret_cast<void*>(&VC4CL_clEnqueueSVMFreeARM), /* clEnqueueSVMFree */
	reinterpret_cast<void*>(&VC4CL_clEnqueueSVMMemcpyARM), /* clEnqueueSVMMemcpy */
	reinterpret_cast<void*>(&VC4CL_clEnqueueSVMMemFillARM), /* clEnqueueSVMMemFill */
	reinterpret_cast<void*>(&VC4CL_clEnqueueSVMMapARM), /* clEnqueueSVMMap */
	reinterpret_cast<void*>(&VC4CL_clEnqueueSVMUnmapARM), /* clEnqueueSVMUnmap */
	nullptr, /* clCreateSamplerWithProperties */
	reinterpret_cast<void*>(&VC4CL_clSetKernelArgSVMPointerARM), /* clSetKernelArgSVMPointer */
	reinterpret_cast<void*>(&VC4CL_clSetKernelExecInfoARM), /* clSetKernelExecInfo */

	/* cl_khr_sub_groups */
	nullptr, /* clGetKernelSubGroupInfoKHR */

	/* OpenCL 2.1 */
	nullptr, /* clCloneKernel */
	nullptr, /* clCreateProgramWithIL */
	nullptr, /* clEnqueueSVMMigrateMem */
	nullptr, /* clGetDeviceAndHostTimer */
	nullptr, /* clGetHostTimer */
	nullptr, /* clGetKernelSubGroupInfo */
	nullptr, /* clSetDefaultDeviceCommandQueue */

	/* OpenCL 2.2 */
	nullptr, /* clSetProgramReleaseCallback */
	nullptr /* clSetProgramSpecializationConstant */
};

#endif
