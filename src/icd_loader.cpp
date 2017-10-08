/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <CL/opencl.h>

#include "common.h"
#include "extensions.h"
#include "icd_loader.h"
#include "Platform.h"
#include "Device.h"


/*
 * Specification for the ICD loader:
 * https://www.khronos.org/registry/OpenCL/extensions/khr/cl_khr_icd.txt
 * 
 * "The ICD Loader queries the following functions from the library:
 * clIcdGetPlatformIDsKHR, clGetPlatformInfo, and clGetExtensionFunctionAddress."
 */

cl_int VC4CL_FUNC(clIcdGetPlatformIDsKHR)(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms)
{
	if ((num_entries == 0) != (platforms == NULL))
		return vc4cl::returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameter is empty!");

	if (num_platforms != NULL) {
		*num_platforms = 1;
	}
	if (platforms != NULL) {
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
	(void*)&VC4CL_clGetPlatformIDs, /* clGetPlatformIDs */
	(void*)&VC4CL_clGetPlatformInfo, /* clGetPlatformInfo */
	(void*)&VC4CL_clGetDeviceIDs, /* clGetDeviceIDs */
	(void*)&VC4CL_clGetDeviceInfo, /* clGetDeviceInfo */
	(void*)&VC4CL_clCreateContext, /* clCreateContext */
	(void*)&VC4CL_clCreateContextFromType, /* clCreateContextFromType */
	(void*)&VC4CL_clRetainContext, /* clRetainContext */
	(void*)&VC4CL_clReleaseContext, /* clReleaseContext */
	(void*)&VC4CL_clGetContextInfo, /* clGetContextInfo */
	(void*)&VC4CL_clCreateCommandQueue, /* clCreateCommandQueue */
	(void*)&VC4CL_clRetainCommandQueue, /* clRetainCommandQueue */
	(void*)&VC4CL_clReleaseCommandQueue, /* clReleaseCommandQueue */
	(void*)&VC4CL_clGetCommandQueueInfo, /* clGetCommandQueueInfo */
	(void*)&VC4CL_clSetCommandQueueProperty, /* clSetCommandQueueProperty */
	(void*)&VC4CL_clCreateBuffer, /* clCreateBuffer */
	(void*)&VC4CL_clCreateImage2D, /* clCreateImage2D */
	(void*)&VC4CL_clCreateImage3D, /* clCreateImage3D */
	(void*)&VC4CL_clRetainMemObject, /* clRetainMemObject */
	(void*)&VC4CL_clReleaseMemObject, /* clReleaseMemObject */
	(void*)&VC4CL_clGetSupportedImageFormats, /* clGetSupportedImageFormats */
	(void*)&VC4CL_clGetMemObjectInfo, /* clGetMemObjectInfo */
	(void*)&VC4CL_clGetImageInfo, /* clGetImageInfo */
	(void*)&VC4CL_clCreateSampler, /* clCreateSampler */
	(void*)&VC4CL_clRetainSampler, /* clRetainSampler */
	(void*)&VC4CL_clReleaseSampler, /* clReleaseSampler */
	(void*)&VC4CL_clGetSamplerInfo, /* clGetSamplerInfo */
	(void*)&VC4CL_clCreateProgramWithSource, /* clCreateProgramWithSource */
	(void*)&VC4CL_clCreateProgramWithBinary, /* clCreateProgramWithBinary */
	(void*)&VC4CL_clRetainProgram, /* clRetainProgram */
	(void*)&VC4CL_clReleaseProgram, /* clReleaseProgram */
	(void*)&VC4CL_clBuildProgram, /* clBuildProgram */
	(void*)&VC4CL_clUnloadCompiler, /* clUnloadCompiler */
	(void*)&VC4CL_clGetProgramInfo, /* clGetProgramInfo */
	(void*)&VC4CL_clGetProgramBuildInfo, /* clGetProgramBuildInfo */
	(void*)&VC4CL_clCreateKernel, /* clCreateKernel */
	(void*)&VC4CL_clCreateKernelsInProgram, /* clCreateKernelsInProgram */
	(void*)&VC4CL_clRetainKernel, /* clRetainKernel */
	(void*)&VC4CL_clReleaseKernel, /* clReleaseKernel */
	(void*)&VC4CL_clSetKernelArg, /* clSetKernelArg */
	(void*)&VC4CL_clGetKernelInfo, /* clGetKernelInfo */
	(void*)&VC4CL_clGetKernelWorkGroupInfo, /* clGetKernelWorkGroupInfo */
	(void*)&VC4CL_clWaitForEvents, /* clWaitForEvents */
	(void*)&VC4CL_clGetEventInfo, /* clGetEventInfo */
	(void*)&VC4CL_clRetainEvent, /* clRetainEvent */
	(void*)&VC4CL_clReleaseEvent, /* clReleaseEvent */
	(void*)&VC4CL_clGetEventProfilingInfo, /* clGetEventProfilingInfo */
	(void*)	&VC4CL_clFlush, /* clFlush */
	(void*)&VC4CL_clFinish, /* clFinish */
	(void*)&VC4CL_clEnqueueReadBuffer, /* clEnqueueReadBuffer */
	(void*)&VC4CL_clEnqueueWriteBuffer, /* clEnqueueWriteBuffer */
	(void*)&VC4CL_clEnqueueCopyBuffer, /* clEnqueueCopyBuffer */
	(void*)&VC4CL_clEnqueueReadImage, /* clEnqueueReadImage */
	(void*)&VC4CL_clEnqueueWriteImage, /* clEnqueueWriteImage */
	(void*)&VC4CL_clEnqueueCopyImage, /* clEnqueueCopyImage */
	(void*)&VC4CL_clEnqueueCopyImageToBuffer, /* clEnqueueCopyImageToBuffer */
	(void*)&VC4CL_clEnqueueCopyBufferToImage, /* clEnqueueCopyBufferToImage */
	(void*)&VC4CL_clEnqueueMapBuffer, /* clEnqueueMapBuffer */
	(void*)&VC4CL_clEnqueueMapImage, /* clEnqueueMapImage */
	(void*)&VC4CL_clEnqueueUnmapMemObject, /* clEnqueueUnmapMemObject */
	(void*)&VC4CL_clEnqueueNDRangeKernel, /* clEnqueueNDRangeKernel */
	(void*)&VC4CL_clEnqueueTask, /* clEnqueueTask */
	(void*)&VC4CL_clEnqueueNativeKernel, /* clEnqueueNativeKernel */
	(void*)&VC4CL_clEnqueueMarker, /* clEnqueueMarker */
	(void*)&VC4CL_clEnqueueWaitForEvents, /* clEnqueueWaitForEvents */
	(void*)&VC4CL_clEnqueueBarrier, /* clEnqueueBarrier */
	(void*)&clGetExtensionFunctionAddress, /* clGetExtensionFunctionAddress */
	
	/* cl_khr_gl_sharing */
	NULL, /* clCreateFromGLBuffer */
	NULL, /* clCreateFromGLTexture2D */
	NULL, /* clCreateFromGLTexture3D */
	NULL, /* clCreateFromGLRenderbuffer */
	NULL, /* clGetGLObjectInfo */
	NULL, /* clGetGLTextureInfo */
	NULL, /* clEnqueueAcquireGLObjects */
	NULL, /* clEnqueueReleaseGLObjects */
	NULL, /* clGetGLContextInfoKHR */

	/* cl_khr_d3d10_sharing */
	NULL, /* clGetDeviceIDsFromD3D10KHR */
	NULL, /* clCreateFromD3D10BufferKHR */
	NULL, /* clCreateFromD3D10Texture2DKHR */
	NULL, /* clCreateFromD3D10Texture3DKHR */
	NULL, /* clEnqueueAcquireD3D10ObjectsKHR */
	NULL, /* clEnqueueReleaseD3D10ObjectsKHR */

	/* OpenCL 1.1 */
	(void*)&VC4CL_clSetEventCallback, /* clSetEventCallback */
	(void*)&VC4CL_clCreateSubBuffer, /* clCreateSubBuffer */
	(void*)&VC4CL_clSetMemObjectDestructorCallback, /* clSetMemObjectDestructorCallback */
	(void*)&VC4CL_clCreateUserEvent, /* clCreateUserEvent */
	(void*)&VC4CL_clSetUserEventStatus, /* clSetUserEventStatus */
	(void*)&VC4CL_clEnqueueReadBufferRect, /* clEnqueueReadBufferRect */
	(void*)&VC4CL_clEnqueueWriteBufferRect, /* clEnqueueWriteBufferRect */
	(void*)&VC4CL_clEnqueueCopyBufferRect, /* clEnqueueCopyBufferRect */

	/* cl_ext_device_fission */
	NULL, /* clCreateSubDevicesEXT */
	NULL, /* clRetainDeviceEXT */
	NULL, /* clReleaseDeviceEXT */

	/* cl_khr_gl_event */
	NULL, /* clCreateEventFromGLsyncKHR */

	/* OpenCL 1.2 */
	(void*)&VC4CL_clCreateSubDevices, /* clCreateSubDevices */
	(void*)&VC4CL_clRetainDevice, /* clRetainDevice */
	(void*)&VC4CL_clReleaseDevice, /* clReleaseDevice */
	(void*)&VC4CL_clCreateImage, /* clCreateImage */
	(void*)&VC4CL_clCreateProgramWithBuiltInKernels, /* clCreateProgramWithBuiltInKernels */
	(void*)&VC4CL_clCompileProgram, /* clCompileProgram */
	(void*)&VC4CL_clLinkProgram, /* clLinkProgram */
	(void*)&VC4CL_clUnloadPlatformCompiler, /* clUnloadPlatformCompiler */
	(void*)&VC4CL_clGetKernelArgInfo, /* clGetKernelArgInfo */
	(void*)&VC4CL_clEnqueueFillBuffer, /* clEnqueueFillBuffer */
	(void*)&VC4CL_clEnqueueFillImage, /* clEnqueueFillImage */
	(void*)&VC4CL_clEnqueueMigrateMemObjects, /* clEnqueueMigrateMemObjects */
	(void*)&VC4CL_clEnqueueMarkerWithWaitList, /* clEnqueueMarkerWithWaitList */
	(void*)&VC4CL_clEnqueueBarrierWithWaitList, /* clEnqueueBarrierWithWaitList */
	(void*)&VC4CL_clGetExtensionFunctionAddressForPlatform, /* clGetExtensionFunctionAddressForPlatform */
	
	/* cl_khr_gl_sharing */
	NULL, /* clCreateFromGLTexture */

	/* cl_khr_d3d11_sharing */
	NULL, /* clGetDeviceIDsFromD3D11KHR */
	NULL, /* clCreateFromD3D11BufferKHR */
	NULL, /* clCreateFromD3D11Texture2DKHR */
	NULL, /* clCreateFromD3D11Texture3DKHR */
	NULL, /* clCreateFromDX9MediaSurfaceKHR */
	NULL, /* clEnqueueAcquireD3D11ObjectsKHR */
	NULL, /* clEnqueueReleaseD3D11ObjectsKHR */

	/* cl_khr_dx9_media_sharing */
	NULL, /* clGetDeviceIDsFromDX9MediaAdapterKHR */
	NULL, /* clEnqueueAcquireDX9MediaSurfacesKHR */
	NULL, /* clEnqueueReleaseDX9MediaSurfacesKHR */

	/* cl_khr_egl_image */
	NULL, /* clCreateFromEGLImageKHR */
	NULL, /* clEnqueueAcquireEGLObjectsKHR */
	NULL, /* clEnqueueReleaseEGLObjectsKHR */

	/* cl_khr_egl_event */
	NULL, /* clCreateEventFromEGLSyncKHR */

	/* OpenCL 2.0 */
	NULL, /* clCreateCommandQueueWithProperties */
	NULL, /* clCreatePipe */
	NULL, /* clGetPipeInfo */
	(void*)&VC4CL_clSVMAllocARM, /* clSVMAlloc */
	(void*)&VC4CL_clSVMFreeARM, /* clSVMFree */
	(void*)&VC4CL_clEnqueueSVMFreeARM, /* clEnqueueSVMFree */
	(void*)&VC4CL_clEnqueueSVMMemcpyARM, /* clEnqueueSVMMemcpy */
	(void*)&VC4CL_clEnqueueSVMMemFillARM, /* clEnqueueSVMMemFill */
	(void*)&VC4CL_clEnqueueSVMMapARM, /* clEnqueueSVMMap */
	(void*)&VC4CL_clEnqueueSVMUnmapARM, /* clEnqueueSVMUnmap */
	NULL, /* clCreateSamplerWithProperties */
	(void*)&VC4CL_clSetKernelArgSVMPointerARM, /* clSetKernelArgSVMPointer */
	(void*)&VC4CL_clSetKernelExecInfoARM, /* clSetKernelExecInfo */

	/* cl_khr_sub_groups */
	NULL, /* clGetKernelSubGroupInfoKHR */

	/* OpenCL 2.1 */
	NULL, /* clCloneKernel */
	NULL, /* clCreateProgramWithIL */
	NULL, /* clEnqueueSVMMigrateMem */
	NULL, /* clGetDeviceAndHostTimer */
	NULL, /* clGetHostTimer */
	NULL, /* clGetKernelSubGroupInfo */
	NULL, /* clSetDefaultDeviceCommandQueue */

	/* OpenCL 2.2 */
	NULL, /* clSetProgramReleaseCallback */
	NULL /* clSetProgramSpecializationConstant */
};

#endif
