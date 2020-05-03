/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
/*
 * Collection of OpenCL 2.x functions which are not supported, but need to be present for OpenCL 3.0 support.
 *
 * All functions present in here are part of some OpenCL 3.0 optional feature for which VC4CL returns CL_FALSE in the
 * corresponding check function.
 */
#include "common.h"

using namespace vc4cl;

#ifdef CL_VERSION_2_0 /* these are not really supported, but required to be present for OpenCL 3.0 support */
void* VC4CL_FUNC(clSVMAlloc)(cl_context context, cl_svm_mem_flags flags, size_t size, cl_uint alignment)
{
    // not supported
    return nullptr;
}

void VC4CL_FUNC(clSVMFree)(cl_context context, void* svm_pointer)
{
    // no-op
}

cl_int VC4CL_FUNC(clEnqueueSVMFree)(cl_command_queue command_queue, cl_uint num_svm_pointers, void* svm_pointers[],
    void (*pfn_free_func)(cl_command_queue, cl_uint, void*[], void*), void* user_data, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_int VC4CL_FUNC(clEnqueueSVMMemcpy)(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr,
    const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_int VC4CL_FUNC(clEnqueueSVMMemFill)(cl_command_queue command_queue, void* svm_ptr, const void* pattern,
    size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_int VC4CL_FUNC(clEnqueueSVMMap)(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags flags,
    void* svm_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_int VC4CL_FUNC(clEnqueueSVMUnmap)(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_int VC4CL_FUNC(clSetKernelArgSVMPointer)(cl_kernel kernel, cl_uint arg_index, const void* arg_value)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_int VC4CL_FUNC(clSetKernelExecInfo)(
    cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size, const void* param_value)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_mem VC4CL_FUNC(clCreatePipe)(cl_context context, cl_mem_flags flags, cl_uint pipe_packet_size,
    cl_uint pipe_max_packets, const cl_pipe_properties* properties, cl_int* errcode_ret)
{
    // not supported
    return returnError<cl_mem>(CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Pipe feature is not supported!");
}

cl_int VC4CL_FUNC(clGetPipeInfo)(
    cl_mem pipe, cl_pipe_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    // not supported
    return returnError(CL_INVALID_MEM_OBJECT, __FILE__, __LINE__, "Pipe feature is not supported!");
}

#endif

#ifdef CL_VERSION_2_1 /* these are not really supported, but required to be present for OpenCL 3.0 support */

cl_int VC4CL_FUNC(clEnqueueSVMMigrateMem)(cl_command_queue command_queue, cl_uint num_svm_pointers,
    const void** svm_pointers, const size_t* sizes, cl_mem_migration_flags flags, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "SVM feature is not supported!");
}

cl_int VC4CL_FUNC(clGetKernelSubGroupInfo)(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info param_name,
    size_t input_value_size, const void* input_value, size_t param_value_size, void* param_value,
    size_t* param_value_size_ret)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Subgroup feature is not supported!");
}

cl_int VC4CL_FUNC(clGetHostTimer)(cl_device_id device, cl_ulong* host_timestamp)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Timer synchronization feature is not supported!");
}

cl_int VC4CL_FUNC(clGetDeviceAndHostTimer)(cl_device_id device, cl_ulong* device_timestamp, cl_ulong* host_timestamp)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Timer synchronization feature is not supported!");
}

cl_int VC4CL_FUNC(clSetDefaultDeviceCommandQueue)(
    cl_context context, cl_device_id device, cl_command_queue command_queue)
{
    // not supported
    return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Device-side enqueue feature is not supported!");
}
#endif
