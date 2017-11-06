/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef ICD_H
#define ICD_H

#include "common.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/*
	 * Khronos Intermediate Language Programs (cl_khr_il_program)
	 * https://www.khronos.org/registry/OpenCL/specs/opencl-2.0-extensions.pdf#page=165
	 *
	 * Supports SPIR-V programs for OpenCL 2.1!!
	 */
#define SPIR_V_VERSION "SPIR-V 1.2"
	//somehow, except in the OpenCL 2.0 extension specification, this extension, constant and the function added ("clCreateProgramWithILKHR") are nowhere to be found
	//maybe due to the fact, that they are included in core OpenCL 2.1? -> anyway, define them with the values of the corresponding core-features in OpenCL 2.0
#ifndef CL_DEVICE_IL_VERSION	//Only defined for OpenCL 2.1+
#define CL_DEVICE_IL_VERSION 0x105B
#endif
#ifndef	CL_PROGRAM_IL	//Only defined for OpenCL 2.1+
#define CL_PROGRAM_IL 0x1169
#endif

#ifndef CL_DEVICE_IL_VERSION_KHR
#define CL_DEVICE_IL_VERSION_KHR CL_DEVICE_IL_VERSION
#endif

#ifndef CL_PROGRAM_IL_KHR
#define CL_PROGRAM_IL_KHR CL_PROGRAM_IL
#endif
	cl_program VC4CL_FUNC(clCreateProgramWithILKHR)(cl_context context, const void* il, size_t length, cl_int* errcode_ret);

	/*
	 * Altera device temperature (cl_altera_device_temperature)
	 * https://www.khronos.org/registry/OpenCL/extensions/altera/cl_altera_device_temperature.txt
	 *
	 * Accepted by the <param_name> argument of clGetDeviceInfo:
	 *  CL_DEVICE_CORE_TEMPERATURE_ALTERA        0x40F3
	 *
	 * cl_device_info:   CL_DEVICE_CORE_TEMPERATURE_ALTERA
	 * Return type   :   cl_int
	 * Description   :   The core die temperature of the device, in degrees Celsius. If the device does not support the query, the result will default to 0.
	 */
#ifndef CL_DEVICE_CORE_TEMPERATURE_ALTERA
#define CL_DEVICE_CORE_TEMPERATURE_ALTERA 0x40F3
#endif

	/*
	 * ARM Shared Virtual Memory (SVM) (cl_arm_shared_virtual_memory)
	 * https://www.khronos.org/registry/OpenCL/extensions/arm/cl_arm_shared_virtual_memory.txt
	 *
	 * Ports shared virtual memory to OpenCL < 2.0.
	 * VC4CL supports this, because (as apparently several ARM devices), the Raspberry Pi has a shared memory, which can be accessed anyway directly by both host and GPU
	 *
	 * See OpenCL 2.0, sections 5.6 and 5.9.2
	 */
	typedef cl_bitfield cl_svm_mem_flags_arm;
	typedef cl_uint cl_kernel_exec_info_arm;
	typedef cl_bitfield cl_device_svm_capabilities_arm;
	// To be used by clGetDeviceInfo
#ifndef CL_DEVICE_SVM_CAPABILITIES_ARM
#define CL_DEVICE_SVM_CAPABILITIES_ARM                  0x40B6
#endif

#ifndef CL_MEM_USES_SVM_POINTER_ARM
#define CL_MEM_USES_SVM_POINTER_ARM                     0x40B7
#endif

	// To be used by clSetKernelExecInfoARM:
#ifndef CL_KERNEL_EXEC_INFO_SVM_PTRS_ARM
#define CL_KERNEL_EXEC_INFO_SVM_PTRS_ARM                0x40B8
#endif
#ifndef CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM_ARM
#define CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM_ARM   0x40B9
#endif

	// To be used by clGetEventInfo:
#ifndef CL_COMMAND_SVM_FREE_ARM
#define CL_COMMAND_SVM_FREE_ARM                         0x40BA
#endif
#ifndef CL_COMMAND_SVM_MEMCPY_ARM
#define CL_COMMAND_SVM_MEMCPY_ARM                       0x40BB
#endif
#ifndef CL_COMMAND_SVM_MEMFILL_ARM
#define CL_COMMAND_SVM_MEMFILL_ARM                      0x40BC
#endif
#ifndef CL_COMMAND_SVM_MAP_ARM
#define CL_COMMAND_SVM_MAP_ARM                          0x40BD
#endif
#ifndef CL_COMMAND_SVM_UNMAP_ARM
#define CL_COMMAND_SVM_UNMAP_ARM                        0x40BE
#endif

	// Flag values returned by clGetDeviceInfo with CL_DEVICE_SVM_CAPABILITIES_ARM as the param_name.
#ifndef CL_DEVICE_SVM_COARSE_GRAIN_BUFFER_ARM
#define CL_DEVICE_SVM_COARSE_GRAIN_BUFFER_ARM           (1 << 0)
#endif
#ifndef CL_DEVICE_SVM_FINE_GRAIN_BUFFER_ARM
#define CL_DEVICE_SVM_FINE_GRAIN_BUFFER_ARM             (1 << 1)
#endif
#ifndef CL_DEVICE_SVM_FINE_GRAIN_SYSTEM_ARM
#define CL_DEVICE_SVM_FINE_GRAIN_SYSTEM_ARM             (1 << 2)
#endif
#ifndef CL_DEVICE_SVM_ATOMICS_ARM
#define CL_DEVICE_SVM_ATOMICS_ARM                       (1 << 3)
#endif

	// Flag values used by clSVMAllocARM:
#ifndef CL_MEM_SVM_FINE_GRAIN_BUFFER_ARM
#define CL_MEM_SVM_FINE_GRAIN_BUFFER_ARM                (1 << 10)
#endif
#ifndef CL_MEM_SVM_ATOMICS_ARM
#define CL_MEM_SVM_ATOMICS_ARM                          (1 << 11)
#endif

	void* VC4CL_FUNC(clSVMAllocARM)(cl_context context, cl_svm_mem_flags_arm flags, size_t size, cl_uint alignment);
	void VC4CL_FUNC(clSVMFreeARM)(cl_context context, void* svm_pointer);
	cl_int VC4CL_FUNC(clEnqueueSVMFreeARM)(cl_command_queue command_queue, cl_uint num_svm_pointers, void* svm_pointers[], void (CL_CALLBACK*pfn_free_func)(cl_command_queue, cl_uint, void*[], void*), void* user_data,
			cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
	cl_int VC4CL_FUNC(clEnqueueSVMMemcpyARM)(cl_command_queue command_queue, cl_bool blocking_copy, void* dst_ptr, const void* src_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
	cl_int VC4CL_FUNC(clEnqueueSVMMemFillARM)(cl_command_queue command_queue, void* svm_ptr, const void* pattern, size_t pattern_size, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
	cl_int VC4CL_FUNC(clEnqueueSVMMapARM)(cl_command_queue command_queue, cl_bool blocking_map, cl_map_flags map_flags, void* svm_ptr, size_t size, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
	cl_int VC4CL_FUNC(clEnqueueSVMUnmapARM)(cl_command_queue command_queue, void* svm_ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event);
	cl_int VC4CL_FUNC(clSetKernelArgSVMPointerARM)(cl_kernel kernel, cl_uint arg_index, const void* arg_value);
	cl_int VC4CL_FUNC(clSetKernelExecInfoARM)(cl_kernel kernel, cl_kernel_exec_info_arm param_name, size_t param_value_size, const void* param_value);

	/*
	 * Support for packed yuv images (cl_intel_packed_yuv)
	 * https://www.khronos.org/registry/OpenCL/extensions/intel/cl_intel_packed_yuv.txt
	 *
	 * Supports packed YUV images as image-formats
	 */
#ifndef CL_YUYV_INTEL
#define CL_YUYV_INTEL	0x4076
#endif
#ifndef CL_UYVY_INTEL
#define CL_UYVY_INTEL	0x4077
#endif
#ifndef CL_YVYU_INTEL
#define CL_YVYU_INTEL	0x4078
#endif
#ifndef CL_VYUY_INTEL
#define CL_VYUY_INTEL	0x4079
#endif

	/*
	 * VC4CL performance counters (cl_vc4cl_performance_counters)
	 */

#define CL_COUNTER_IDLE_CYCLES_VC4CL 13
#define CL_COUNTER_EXECUTION_CYCLES_VC4CL 16
#define CL_COUNTER_TMU_STALL_CYCLES_VC4CL 17
#define CL_COUNTER_INSTRUCTION_CACHE_HITS_VC4CL 20
#define CL_COUNTER_INSTRUCTION_CACHE_MISSES_VC4CL 21
#define CL_COUNTER_ARGUMENT_CACHE_HITS_VC4CL 22
#define CL_COUNTER_ARGUMENT_CACHE_MISSES_VC4CL 23
#define CL_COUNTER_MEMORY_WRITE_STALL_CYCES_VC4CL 26
#define CL_COUNTER_MEMORY_READ_STALL_CYCLES_VC4CL 27
#define CL_COUNTER_L2_CACHE_HITS_VC4CL 28
#define CL_COUNTER_L2_CACHE_MISSES_VC4CL 29

#define CL_INVALID_PERFORMANCE_COUNTER -112

	typedef cl_uchar cl_counter_type_vc4cl;
	typedef struct _cl_counter_vc4cl* cl_counter_vc4cl;

	typedef CL_API_ENTRY cl_counter_vc4cl (CL_API_CALL *clCreatePerformanceCounterVC4CL_fn)(cl_device_id device, const cl_counter_type_vc4cl counter_type, cl_int* errcode_ret);
	cl_counter_vc4cl VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(cl_device_id device, const cl_counter_type_vc4cl counter_type, cl_int* errcode_ret);

	typedef CL_API_ENTRY cl_int (CL_API_CALL *clGetPerformanceCounterValueVC4CL_fn)(cl_counter_vc4cl counter, cl_uint* value);
	cl_int VC4CL_FUNC(clGetPerformanceCounterValueVC4CL)(cl_counter_vc4cl counter, cl_uint* value);

	typedef CL_API_ENTRY cl_int (CL_API_CALL *clReleasePerformanceCounterVC4CL_fn)(cl_counter_vc4cl counter);
	cl_int VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(cl_counter_vc4cl counter);

	typedef CL_API_ENTRY cl_int (CL_API_CALL *clRetainPerformanceCounterVC4CL_fn)(cl_counter_vc4cl counter);
	cl_int VC4CL_FUNC(clRetainPerformanceCounterVC4CL)(cl_counter_vc4cl counter);

	typedef CL_API_ENTRY cl_int (CL_API_CALL *clResetPerformanceCounterValueVC4CL_fn)(cl_counter_vc4cl counter);
	cl_int VC4CL_FUNC(clResetPerformanceCounterValueVC4CL)(cl_counter_vc4cl counter);

#ifdef __cplusplus
}
#endif

#endif /* ICD_H */

