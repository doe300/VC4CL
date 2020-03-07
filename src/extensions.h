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
 * Khronos SPIR Binaries (cl_khr_spir)
 * https://www.khronos.org/registry/OpenCL/specs/opencl-1.2-extensions.pdf#page=135
 *
 * "clCreateProgramWithBinary can be used to load a SPIR binary. Once a program object has been created from a SPIR
 * binary, clBuildProgram can be called to build a program executable or clCompileProgram can be called to compile the
 * SPIR binary."
 */
// SPIRV-LLVM and "standard" CLang (4.0) support SPIR 1.2 (for OpenCL 1.2)
#define SPIR_PREFIX "SPIR_"
#define SPIR_VERSION "1.2"
#define SPIR_VERSION_MAJOR 1
#define SPIR_VERSION_MINOR 2

/*
 * Khronos Intermediate Language Programs (cl_khr_il_program)
 * https://www.khronos.org/registry/OpenCL/specs/opencl-1.2-extensions.pdf#page=165
 */
// SPIR-V 1.5 is the currently latest SPIR-V version
#define SPIRV_VERSION "SPIR-V_1.5"
#define SPIRV_VERSION_MAJOR 1
#define SPIRV_VERSION_MINOR 5

// somehow, except in the OpenCL 2.0 extension specification, this extension, constant and the function added
// ("clCreateProgramWithILKHR") are nowhere to be found  maybe due to the fact, that they are included in core
// OpenCL 2.1?
// -> anyway, define them with the values of the corresponding core-features in OpenCL 2.0  with PR
// https://github.com/KhronosGroup/OpenCL-Headers/pull/24 they are added to the official OpenCL headers repository with
// the same values
#ifndef CL_DEVICE_IL_VERSION // Only defined for OpenCL 2.1+
#define CL_DEVICE_IL_VERSION 0x105B
#endif
#ifndef CL_PROGRAM_IL // Only defined for OpenCL 2.1+
#define CL_PROGRAM_IL 0x1169
#endif

#ifndef CL_DEVICE_IL_VERSION_KHR
#define CL_DEVICE_IL_VERSION_KHR CL_DEVICE_IL_VERSION
#endif

#ifndef CL_PROGRAM_IL_KHR
#define CL_PROGRAM_IL_KHR CL_PROGRAM_IL
#endif
    cl_program VC4CL_FUNC(clCreateProgramWithILKHR)(
        cl_context context, const void* il, size_t length, cl_int* errcode_ret);

/*
 * Khronos local and private memory initialization (cl_khr_initialize_memory)
 * OpenCL 1.2 extension specification, section 9.15
 *
 * Enables additional context-properties to initialize local and private memory with zeroes.
 *
 * NOTE: Local memory is automatically initialized to zero, if the local allocation has no initial value.
 */
#ifndef CL_CONTEXT_MEMORY_INITIALIZE_KHR
#define CL_CONTEXT_MEMORY_INITIALIZE_KHR 0x2030
#endif

#ifndef CL_CONTEXT_MEMORY_INITIALIZE_LOCAL_KHR
#define CL_CONTEXT_MEMORY_INITIALIZE_LOCAL_KHR 0x1 // XXX correct value?
#endif
#ifndef CL_CONTEXT_MEMORY_INITIALIZE_PRIVATE_KHR
#define CL_CONTEXT_MEMORY_INITIALIZE_PRIVATE_KHR 0x2 // XXX correct value?
#endif

/*
 * Khronos creating command queues with properties (cl_khr_create_command_queue)
 * https://github.com/KhronosGroup/OpenCL-Docs/blob/master/ext/cl_khr_create_command_queue.txt
 *
 * Enables creating of command queues with properties for OpenCL versions < 2.0.
 */
#ifndef cl_khr_create_command_queue
#define cl_khr_create_command_queue 1
    typedef cl_bitfield cl_queue_properties_khr;
#endif
    cl_command_queue VC4CL_FUNC(clCreateCommandQueueWithPropertiesKHR)(
        cl_context context, cl_device_id device, const cl_queue_properties_khr* properties, cl_int* errcode_ret);

/*
 * Altera device temperature (cl_altera_device_temperature)
 * https://www.khronos.org/registry/OpenCL/extensions/altera/cl_altera_device_temperature.txt
 *
 * Accepted by the <param_name> argument of clGetDeviceInfo:
 *  CL_DEVICE_CORE_TEMPERATURE_ALTERA        0x40F3
 *
 * cl_device_info:   CL_DEVICE_CORE_TEMPERATURE_ALTERA
 * Return type   :   cl_int
 * Description   :   The core die temperature of the device, in degrees Celsius. If the device does not support the
 * query, the result will default to 0.
 */
#ifndef CL_DEVICE_CORE_TEMPERATURE_ALTERA
#define CL_DEVICE_CORE_TEMPERATURE_ALTERA 0x40F3
#endif

/*
 * Support for packed yuv images (cl_intel_packed_yuv)
 * https://www.khronos.org/registry/OpenCL/extensions/intel/cl_intel_packed_yuv.txt
 *
 * Supports packed YUV images as image-formats
 */
#ifndef CL_YUYV_INTEL
#define CL_YUYV_INTEL 0x4076
#endif
#ifndef CL_UYVY_INTEL
#define CL_UYVY_INTEL 0x4077
#endif
#ifndef CL_YVYU_INTEL
#define CL_YVYU_INTEL 0x4078
#endif
#ifndef CL_VYUY_INTEL
#define CL_VYUY_INTEL 0x4079
#endif

    /*
     * Altera live object tracking (cl_altera_live_object_tracking)
     * https://www.khronos.org/registry/OpenCL/extensions/altera/cl_altera_live_object_tracking.txt
     *
     * Introduces two new runtime-functions to track all currently allocated OpenCL objects
     *
     * Implementation and usage notes:
     *
     * - Also, the runtime may cause some objects to automatically retain other objects, so reference counts may be
     * higher than apparent from host program source code.
     */

    /*!
     * Registers a future interest in enumerating all the live objects in the runtime API.
     * Registering such an interest may itself increase memory use and runtime, which is why is must be explicitly
     * requested.
     *
     * Behaviour is unspecified if the clTrackLiveObjectsAltera method is called before the the first call to
     * clGetPlatformIDs.
     */
    void VC4CL_FUNC(clTrackLiveObjectsAltera)(cl_platform_id platform);
    /*!
     * Requests an enumeration of all live objects in the runtime.  The enumeration is performed by calling the callback
     * function once for each live object in some implementation-defined sequence (i.e. not concurrently).
     *
     * The arguments to clReportLiveObjectsAltera are as follows:
     *
     * \param platform is the platform for which live objects are being tracked.
     *
     * \param report_fn is a callback function. It is called for every live object in the runtime. The arguments to the
     * callback function are:
     * - user_data is the user_data argument specified to clReportLiveObjectsAltera
     * - obj_ptr is a pointer to the live object, cast to type void*. (Note that all OpenCL API objects tracked are
     * type-defined in the OpenCL API header files to be pointers to implementation-defined structs.)
     * - type_name is a C string corresponding to the OpenCL API object type.  For example, a leaked cl_mem object will
     * have "cl_mem" as its type string.
     * - refcount is an instantaneous reference count for the object. Consider it to be immediately stale.
     *
     * \param user_data is a pointer to user supplied data.
     */
    void VC4CL_FUNC(clReportLiveObjectsAltera)(cl_platform_id platform,
        void(CL_CALLBACK* report_fn)(
            void* /* user_data */, void* /* obj_ptr */, const char* /* type_name */, cl_uint /* refcount */),
        void* user_data);

/*
 * ARM_core_id (cl_arm_core_id)
 * https://www.khronos.org/registry/OpenCL/extensions/arm/cl_arm_get_core_id.txt
 *
 * Introduces OpenCL C function to query compute unit id and host-side device query fir present compute units.
 *
 * Implementation and usage notes:
 * - we only have a single compute unit with the constant id zero
 */
#ifndef CL_DEVICE_COMPUTE_UNITS_BITFIELD_ARM
#define CL_DEVICE_COMPUTE_UNITS_BITFIELD_ARM 0x40BF
#endif

/*
 * Khronos extended versioning (cl_khr_extended_versioning)
 * https://github.com/KhronosGroup/OpenCL-Docs/pull/218/files
 *
 * Adds some more queries for better/more accurate versioning support
 */
#ifndef CL_VERSION_MAJOR_BITS_KHR
#define CL_VERSION_MAJOR_BITS_KHR (10u)
#endif
#ifndef CL_VERSION_MINOR_BITS_KHR
#define CL_VERSION_MINOR_BITS_KHR (10u)
#endif
#ifndef CL_VERSION_PATCH_BITS_KHR
#define CL_VERSION_PATCH_BITS_KHR (12u)
#endif
#ifndef CL_VERSION_MAJOR_MASK_KHR
#define CL_VERSION_MAJOR_MASK_KHR ((1u << CL_VERSION_MAJOR_BITS_KHR) - 1u)
#endif
#ifndef CL_VERSION_MINOR_MASK_KHR
#define CL_VERSION_MINOR_MASK_KHR ((1u << CL_VERSION_MINOR_BITS_KHR) - 1u)
#endif
#ifndef CL_VERSION_PATCH_MASK_KHR
#define CL_VERSION_PATCH_MASK_KHR ((1u << CL_VERSION_PATCH_BITS_KHR) - 1u)
#endif
#ifndef CL_VERSION_MAJOR_KHR
#define CL_VERSION_MAJOR_KHR(version) ((version) >> (CL_VERSION_MINOR_BITS_KHR + CL_VERSION_PATCH_BITS_KHR))
#endif
#ifndef CL_VERSION_MINOR_KHR
#define CL_VERSION_MINOR_KHR(version) (((version) >> CL_VERSION_PATCH_BITS_KHR) & CL_VERSION_MINOR_MASK_KHR)
#endif
#ifndef CL_VERSION_PATCH_KHR
#define CL_VERSION_PATCH_KHR(version) ((version) &CL_VERSION_PATCH_MASK_KHR)
#endif
#ifndef CL_MAKE_VERSION_KHR
#define CL_MAKE_VERSION_KHR(major, minor, patch)                                                                       \
    ((((major) &CL_VERSION_MAJOR_MASK_KHR) << (CL_VERSION_MINOR_BITS_KHR + CL_VERSION_PATCH_BITS_KHR)) |               \
        (((minor) &CL_VERSION_MINOR_MASK_KHR) << CL_VERSION_PATCH_BITS_KHR) | ((patch) &CL_VERSION_PATCH_MASK_KHR))
#endif
#ifndef CL_NAME_VERSION_MAX_NAME_SIZE_KHR
#define CL_NAME_VERSION_MAX_NAME_SIZE_KHR 64
    typedef cl_uint cl_version_khr;
    // NOTA: the structure below must be packed so that the OpenCL implementation
    //       and application use the same layout in memory.
    typedef struct _cl_name_version_khr
    {
        cl_version_khr version;
        char name[CL_NAME_VERSION_MAX_NAME_SIZE_KHR];
    } cl_name_version_khr;
#endif
#ifndef CL_PLATFORM_NUMERIC_VERSION_KHR
#define CL_PLATFORM_NUMERIC_VERSION_KHR 0x0906
#endif
#ifndef CL_PLATFORM_EXTENSIONS_WITH_VERSION_KHR
#define CL_PLATFORM_EXTENSIONS_WITH_VERSION_KHR 0x0907
#endif
#ifndef CL_DEVICE_NUMERIC_VERSION_KHR
#define CL_DEVICE_NUMERIC_VERSION_KHR 0x105E
#endif
#ifndef CL_DEVICE_OPENCL_C_NUMERIC_VERSION_KHR
#define CL_DEVICE_OPENCL_C_NUMERIC_VERSION_KHR 0x105F
#endif
#ifndef CL_DEVICE_EXTENSIONS_WITH_VERSION_KHR
#define CL_DEVICE_EXTENSIONS_WITH_VERSION_KHR 0x1060
#endif
#ifndef CL_DEVICE_ILS_WITH_VERSION_KHR
#define CL_DEVICE_ILS_WITH_VERSION_KHR 0x1061
#endif
#ifndef CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION_KHR
#define CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION_KHR 0x1062
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

#define CL_INVALID_PERFORMANCE_COUNTER_VC4CL (-112)

    typedef cl_uchar cl_counter_type_vc4cl;
    typedef struct _cl_counter_vc4cl* cl_counter_vc4cl;

    /*!
     * Initializes one of the system performance-counters to the type specified and
     * returns an object representing this counter. Returns NULL and sets errcode_ret on error
     */
    cl_counter_vc4cl VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(
        cl_device_id device, cl_counter_type_vc4cl counter_type, cl_int* errcode_ret);
    typedef CL_API_ENTRY cl_counter_vc4cl(CL_API_CALL* clCreatePerformanceCounterVC4CL_fn)(
        cl_device_id device, cl_counter_type_vc4cl counter_type, cl_int* errcode_ret);

    /*!
     * Reads the current value of the performance-counter object passed as argument and stores it into the value
     * output-parameter. Returns the status of the read (CL_SUCCESS on success)
     */
    cl_int VC4CL_FUNC(clGetPerformanceCounterValueVC4CL)(cl_counter_vc4cl counter, cl_uint* value);
    typedef CL_API_ENTRY cl_int(CL_API_CALL* clGetPerformanceCounterValueVC4CL_fn)(
        cl_counter_vc4cl counter, cl_uint* value);

    /*!
     * Decreases the reference counter of the performance-counter object.
     * IF the reference count becomes zero, the system performance-counter associated with this object is released
     * and the memory used by the given object is freed.
     */
    cl_int VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(cl_counter_vc4cl counter);
    typedef CL_API_ENTRY cl_int(CL_API_CALL* clReleasePerformanceCounterVC4CL_fn)(cl_counter_vc4cl counter);

    /*!
     * Increases the reference-counter of the performance-counter object
     */
    cl_int VC4CL_FUNC(clRetainPerformanceCounterVC4CL)(cl_counter_vc4cl counter);
    typedef CL_API_ENTRY cl_int(CL_API_CALL* clRetainPerformanceCounterVC4CL_fn)(cl_counter_vc4cl counter);

    /*
     * Resets the counter-value of the system performance-counter associated with this object to zero.
     */
    cl_int VC4CL_FUNC(clResetPerformanceCounterValueVC4CL)(cl_counter_vc4cl counter);
    typedef CL_API_ENTRY cl_int(CL_API_CALL* clResetPerformanceCounterValueVC4CL_fn)(cl_counter_vc4cl counter);

#ifdef __cplusplus
}
#endif

#endif /* ICD_H */
