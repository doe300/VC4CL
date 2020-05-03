/* This header contains a list of all exported functions to be referenced e.g. for the ICD loader
 *
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef ICD_LOADER_H
#define ICD_LOADER_H

// These functions are declared and defined for backwards compatibility anyway
#define CL_USE_DEPRECATED_OPENCL_1_0_APIS
#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include "vc4cl_config.h"

#ifdef __cplusplus
extern "C"
{
#endif

#if use_cl_khr_icd
#if __has_include("CL/cl_icd.h")
// when using custom OpenCL headers
#include "CL/cl_icd.h"
#elif __has_include(<ocl_icd.h>)
#include <ocl_icd.h>
#else
#error "OpenCL ICD header not found, try installing 'ocl-icd-dev'"
#endif
#define vc4cl_icd_dispatch struct _cl_icd_dispatch* dispatch = &vc4cl_dispatch;
// this idea is stolen from: https://github.com/pocl/pocl/blob/master/lib/CL/pocl_cl.h
#define VC4CL_FUNC(name) VC4CL_##name
    extern struct _cl_icd_dispatch vc4cl_dispatch;
#else
#define vc4cl_icd_dispatch
#define VC4CL_FUNC(name) name
#endif

    cl_int VC4CL_FUNC(clGetPlatformIDs)(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms);
    cl_int VC4CL_FUNC(clGetPlatformInfo)(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);

    cl_int VC4CL_FUNC(clGetDeviceIDs)(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries,
        cl_device_id* devices, cl_uint* num_devices);
    cl_int VC4CL_FUNC(clGetDeviceInfo)(cl_device_id device, cl_device_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clCreateSubDevices)(cl_device_id in_device, const cl_device_partition_property* properties,
        cl_uint num_devices, cl_device_id* out_devices, cl_uint* num_devices_ret);
    cl_int VC4CL_FUNC(clRetainDevice)(cl_device_id device);
    cl_int VC4CL_FUNC(clReleaseDevice)(cl_device_id device);

    cl_context VC4CL_FUNC(clCreateContext)(const cl_context_properties* properties, cl_uint num_devices,
        const cl_device_id* devices,
        void(CL_CALLBACK* pfn_notify)(const char* errinfo, const void* private_info, size_t cb, void* user_data),
        void* user_data, cl_int* errcode_ret);
    cl_context VC4CL_FUNC(clCreateContextFromType)(const cl_context_properties* properties, cl_device_type device_type,
        void(CL_CALLBACK* pfn_notify)(const char* errinfo, const void* private_info, size_t cb, void* user_data),
        void* user_data, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clRetainContext)(cl_context context);
    cl_int VC4CL_FUNC(clReleaseContext)(cl_context context);
    cl_int VC4CL_FUNC(clGetContextInfo)(cl_context context, cl_context_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);

    cl_command_queue VC4CL_FUNC(clCreateCommandQueue)(
        cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clRetainCommandQueue)(cl_command_queue command_queue);
    cl_int VC4CL_FUNC(clReleaseCommandQueue)(cl_command_queue command_queue);
    cl_int VC4CL_FUNC(clGetCommandQueueInfo)(cl_command_queue command_queue, cl_command_queue_info param_name,
        size_t param_value_size, void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clSetCommandQueueProperty)(cl_command_queue command_queue, cl_command_queue_properties properties,
        cl_bool enable, cl_command_queue_properties* old_properties);
    cl_int VC4CL_FUNC(clFlush)(cl_command_queue command_queue);
    cl_int VC4CL_FUNC(clFinish)(cl_command_queue command_queue);

    cl_mem VC4CL_FUNC(clCreateBuffer)(
        cl_context context, cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret);
    cl_mem VC4CL_FUNC(clCreateSubBuffer)(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type buffer_create_type,
        const void* buffer_create_info, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clEnqueueReadBuffer)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
        size_t offset, size_t size, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
        cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueWriteBuffer)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
        size_t offset, size_t size, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
        cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueReadBufferRect)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
        const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch,
        size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, void* ptr,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueWriteBufferRect)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
        const size_t* buffer_origin, const size_t* host_origin, const size_t* region, size_t buffer_row_pitch,
        size_t buffer_slice_pitch, size_t host_row_pitch, size_t host_slice_pitch, const void* ptr,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueCopyBuffer)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer,
        size_t src_offset, size_t dst_offset, size_t size, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueCopyBufferRect)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_buffer,
        const size_t* src_origin, const size_t* dst_origin, const size_t* region, size_t src_row_pitch,
        size_t src_slice_pitch, size_t dst_row_pitch, size_t dst_slice_pitch, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueFillBuffer)(cl_command_queue command_queue, cl_mem buffer, const void* pattern,
        size_t pattern_size, size_t offset, size_t size, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    void* VC4CL_FUNC(clEnqueueMapBuffer)(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map,
        cl_map_flags map_flags, size_t offset, size_t size, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clRetainMemObject)(cl_mem memobj);
    cl_int VC4CL_FUNC(clReleaseMemObject)(cl_mem memobj);
    cl_int VC4CL_FUNC(clSetMemObjectDestructorCallback)(
        cl_mem memobj, void(CL_CALLBACK* pfn_notify)(cl_mem memobj, void* user_data), void* user_data);
    cl_int VC4CL_FUNC(clEnqueueUnmapMemObject)(cl_command_queue command_queue, cl_mem memobj, void* mapped_ptr,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueMigrateMemObjects)(cl_command_queue command_queue, cl_uint num_mem_objects,
        const cl_mem* mem_objects, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clGetMemObjectInfo)(cl_mem memobj, cl_mem_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);

    cl_mem VC4CL_FUNC(clCreateImage)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format,
        const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret);
    cl_mem VC4CL_FUNC(clCreateImage2D)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format,
        size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, cl_int* errcode_ret);
    cl_mem VC4CL_FUNC(clCreateImage3D)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format,
        size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch,
        void* host_ptr, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clGetSupportedImageFormats)(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type,
        cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats);
    cl_int VC4CL_FUNC(clEnqueueReadImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read,
        const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueWriteImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write,
        const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueCopyImage)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image,
        const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueFillImage)(cl_command_queue command_queue, cl_mem image, const void* fill_color,
        const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
        cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueCopyImageToBuffer)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer,
        const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueCopyBufferToImage)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image,
        size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    void* VC4CL_FUNC(clEnqueueMapImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_map,
        cl_map_flags map_flags, const size_t* origin, const size_t* region, size_t* image_row_pitch,
        size_t* image_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event,
        cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clGetImageInfo)(cl_mem image, cl_image_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);

    cl_sampler VC4CL_FUNC(clCreateSampler)(cl_context context, cl_bool normalized_coords,
        cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clRetainSampler)(cl_sampler sampler);
    cl_int VC4CL_FUNC(clReleaseSampler)(cl_sampler sampler);
    cl_int VC4CL_FUNC(clGetSamplerInfo)(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);

    cl_program VC4CL_FUNC(clCreateProgramWithSource)(
        cl_context context, cl_uint count, const char** strings, const size_t* lengths, cl_int* errcode_ret);
    cl_program VC4CL_FUNC(clCreateProgramWithBinary)(cl_context context, cl_uint num_devices,
        const cl_device_id* device_list, const size_t* lengths, const unsigned char** binaries, cl_int* binary_status,
        cl_int* errcode_ret);
    cl_program VC4CL_FUNC(clCreateProgramWithBuiltInKernels)(cl_context context, cl_uint num_devices,
        const cl_device_id* device_list, const char* kernel_names, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clRetainProgram)(cl_program program);
    cl_int VC4CL_FUNC(clReleaseProgram)(cl_program program);
    cl_int VC4CL_FUNC(clBuildProgram)(cl_program program, cl_uint num_devices, const cl_device_id* device_list,
        const char* options, void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data);
    cl_int VC4CL_FUNC(clCompileProgram)(cl_program program, cl_uint num_devices, const cl_device_id* device_list,
        const char* options, cl_uint num_input_headers, const cl_program* input_headers,
        const char** header_include_names, void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data),
        void* user_data);
    cl_program VC4CL_FUNC(clLinkProgram)(cl_context context, cl_uint num_devices, const cl_device_id* device_list,
        const char* options, cl_uint num_input_programs, const cl_program* input_programs,
        void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clUnloadPlatformCompiler)(cl_platform_id platform);
    cl_int VC4CL_FUNC(clUnloadCompiler)(void);
    cl_int VC4CL_FUNC(clGetProgramInfo)(cl_program program, cl_program_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clGetProgramBuildInfo)(cl_program program, cl_device_id device, cl_program_build_info param_name,
        size_t param_value_size, void* param_value, size_t* param_value_size_ret);

    cl_kernel VC4CL_FUNC(clCreateKernel)(cl_program program, const char* kernel_name, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clCreateKernelsInProgram)(
        cl_program program, cl_uint num_kernels, cl_kernel* kernels, cl_uint* num_kernels_ret);
    cl_int VC4CL_FUNC(clRetainKernel)(cl_kernel kernel);
    cl_int VC4CL_FUNC(clReleaseKernel)(cl_kernel kernel);
    cl_int VC4CL_FUNC(clSetKernelArg)(cl_kernel kernel, cl_uint arg_index, size_t arg_size, const void* arg_value);
    cl_int VC4CL_FUNC(clGetKernelInfo)(cl_kernel kernel, cl_kernel_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clGetKernelWorkGroupInfo)(cl_kernel kernel, cl_device_id device,
        cl_kernel_work_group_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clGetKernelArgInfo)(cl_kernel kernel, cl_uint arg_index, cl_kernel_arg_info param_name,
        size_t param_value_size, void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clEnqueueNDRangeKernel)(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim,
        const size_t* global_work_offset, const size_t* global_work_size, const size_t* local_work_size,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueTask)(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueNativeKernel)(cl_command_queue command_queue, void(CL_CALLBACK* user_func)(void*),
        void* args, size_t cb_args, cl_uint num_mem_objects, const cl_mem* mem_list, const void** args_mem_loc,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);

    cl_int VC4CL_FUNC(clEnqueueMarkerWithWaitList)(cl_command_queue command_queue, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueMarker)(cl_command_queue command_queue, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueBarrierWithWaitList)(cl_command_queue command_queue, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueBarrier)(cl_command_queue command_queue);

    cl_event VC4CL_FUNC(clCreateUserEvent)(cl_context context, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clSetUserEventStatus)(cl_event event, cl_int execution_status);
    cl_int VC4CL_FUNC(clWaitForEvents)(cl_uint num_events, const cl_event* event_list);
    cl_int VC4CL_FUNC(clEnqueueWaitForEvents)(
        cl_command_queue command_queue, cl_uint num_events, const cl_event* event_list);
    cl_int VC4CL_FUNC(clGetEventInfo)(cl_event event, cl_event_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clSetEventCallback)(cl_event event, cl_int command_exec_callback_type,
        void(CL_CALLBACK* pfn_event_notify)(cl_event event, cl_int event_command_exec_status, void* user_data),
        void* user_data);
    cl_int VC4CL_FUNC(clRetainEvent)(cl_event event);
    cl_int VC4CL_FUNC(clReleaseEvent)(cl_event event);
    cl_int VC4CL_FUNC(clGetEventProfilingInfo)(cl_event event, cl_profiling_info param_name, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);

    cl_int VC4CL_FUNC(clIcdGetPlatformIDsKHR)(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms);
    void* VC4CL_FUNC(clGetExtensionFunctionAddressForPlatform)(cl_platform_id platform, const char* funcname);

#ifdef CL_VERSION_2_0
    cl_command_queue VC4CL_FUNC(clCreateCommandQueueWithProperties)(
        cl_context context, cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret);
    cl_mem VC4CL_FUNC(clCreatePipe)(cl_context context, cl_mem_flags flags, cl_uint pipe_packet_size,
        cl_uint pipe_max_packets, const cl_pipe_properties* properties, cl_int* errcode_ret);
    cl_int VC4CL_FUNC(clGetPipeInfo)(
        cl_mem pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
    void* VC4CL_FUNC(clSVMAlloc)(cl_context context, cl_svm_mem_flags flags, size_t size, cl_uint alignment);
    void VC4CL_FUNC(clSVMFree)(cl_context context, void* svm_pointer);
    cl_int VC4CL_FUNC(clEnqueueSVMFree)(cl_command_queue command_queue, cl_uint num_svm_pointers, void* svm_pointers[],
        void (*pfn_free_func)(cl_command_queue, cl_uint, void*[], void*), void* user_data,
        cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueSVMMemcpy)(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr,
        const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
        cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueSVMMemFill)(cl_command_queue command_queue, void* svm_ptr, const void* pattern,
        size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
        cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueSVMMap)(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags flags,
        void* svm_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clEnqueueSVMUnmap)(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clSetKernelArgSVMPointer)(cl_kernel kernel, cl_uint arg_index, const void* arg_value);
    cl_int VC4CL_FUNC(clSetKernelExecInfo)(
        cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value);
    cl_sampler VC4CL_FUNC(clCreateSamplerWithProperties)(
        cl_context context, const cl_sampler_properties* sampler_properties, cl_int* errcode_ret);
#endif

    cl_kernel VC4CL_FUNC(clCloneKernel)(cl_kernel source_kernel, cl_int* errcode_ret);

#ifdef CL_VERSION_2_1
    cl_int VC4CL_FUNC(clGetKernelSubGroupInfo)(cl_kernel kernel, cl_device_id device,
        cl_kernel_sub_group_info param_name, size_t input_value_size, const void* input_value, size_t param_value_size,
        void* param_value, size_t* param_value_size_ret);
    cl_int VC4CL_FUNC(clEnqueueSVMMigrateMem)(cl_command_queue command_queue, cl_uint num_svm_pointers,
        const void** svm_pointers, const size_t* sizes, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list,
        const cl_event* event_wait_list, cl_event* event);
    cl_int VC4CL_FUNC(clGetDeviceAndHostTimer)(
        cl_device_id device, cl_ulong* device_timestamp, cl_ulong* host_timestamp);
    cl_int VC4CL_FUNC(clGetHostTimer)(cl_device_id device, cl_ulong* host_timestamp);
    cl_int VC4CL_FUNC(clSetDefaultDeviceCommandQueue)(
        cl_context context, cl_device_id device, cl_command_queue command_queue);
#endif

    cl_int VC4CL_FUNC(clSetProgramSpecializationConstant)(
        cl_program program, cl_uint spec_id, size_t spec_size, const void* spec_value);
    cl_int VC4CL_FUNC(clSetProgramReleaseCallback)(
        cl_program program, void(CL_CALLBACK* pfn_notify)(cl_program program, void* user_data), void* user_data);

#ifdef CL_VERSION_3_0
    cl_mem VC4CL_FUNC(clCreateBufferWithProperties)(cl_context context, const cl_mem_properties* properties,
        cl_mem_flags flags, size_t size, void* host_ptr, cl_int* errcode_ret);
    cl_mem VC4CL_FUNC(clCreateImageWithProperties)(cl_context context, const cl_mem_properties* properties,
        cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr,
        cl_int* errcode_ret);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ICD_LOADER_H */
