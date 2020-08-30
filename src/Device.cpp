/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Device.h"

#include "Mailbox.h"
#include "Platform.h"
#include "V3D.h"
#include "extensions.h"

#include <algorithm>
#include <chrono>

#ifdef COMPILER_HEADER
#include COMPILER_HEADER
#endif

using namespace vc4cl;

Device::~Device() noexcept = default;

cl_int Device::getInfo(
    cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) const
{
    cl_device_partition_property dummy = 0;

    /*
     * if clock period is 1ns -> result is 1
     * for 1 ms -> (1000000000 / 1000) = 1000
     */
    static constexpr auto clockResolution = std::nano::den / std::chrono::high_resolution_clock::period::den;

    switch(param_name)
    {
    case CL_DEVICE_TYPE:
        return returnValue<cl_device_type>(CL_DEVICE_TYPE_GPU, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_VENDOR_ID:
        //"A unique device vendor identifier. An example of a unique device identifier could be the PCIe ID"
        return returnValue<cl_uint>(device_config::VENDOR_ID, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_COMPUTE_UNITS:
        //"The number of parallel compute units on the OpenCL device. A work-group executes on a single compute unit.
        // The minimum value is 1."
        return returnValue<cl_uint>(
            device_config::NUM_COMPUTE_UNITS, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS:
        //"Maximum dimensions that specify the global and local work-item IDs used by the data parallel execution model.
        // The minimum value is 3 for devices that are not of type CL_DEVICE_TYPE_CUSTOM."
        return returnValue<cl_uint>(kernel_config::NUM_DIMENSIONS, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_WORK_ITEM_SIZES:
    {
        //"Maximum number of work-items that can be specified in each dimension of the work-group.
        // Returns n size_t entries, where n is the value returned by the query for CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS.
        // The minimum value is (1, 1, 1)."
        size_t numQPUs = V3D::instance()->getSystemInfo(SystemInfo::QPU_COUNT);
        std::array<size_t, kernel_config::NUM_DIMENSIONS> tmp{numQPUs, numQPUs, numQPUs};
        return returnValue(tmp.data(), sizeof(size_t), tmp.size(), param_value_size, param_value, param_value_size_ret);
    }
    case CL_DEVICE_MAX_WORK_GROUP_SIZE:
        //"Maximum number of work-items in a work-group executing a kernel on a single compute unit, using the data
        // parallel execution model."
        return returnValue<size_t>(
            V3D::instance()->getSystemInfo(SystemInfo::QPU_COUNT), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR:
        //"Preferred native vector width size for built-in scalar types that can be put into vectors.
        // The vector width is defined as the number of scalar elements that can be stored in the vector. "
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR:
        //"Returns the native ISA vector width. The vector width is defined as the number of scalar elements that can be
        // stored in the vector."
        return returnValue<cl_uint>(
            device_config::PREFERRED_VECTOR_WIDTH, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT:
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT:
        return returnValue<cl_uint>(
            device_config::PREFERRED_VECTOR_WIDTH, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT:
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_INT:
        return returnValue<cl_uint>(
            device_config::PREFERRED_VECTOR_WIDTH, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG:
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG:
        // not supported
        return returnValue<cl_uint>(0, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT:
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT:
        return returnValue<cl_uint>(
            device_config::PREFERRED_VECTOR_WIDTH, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE:
        //"If double precision is not supported, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE must return 0."
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE:
        //"If double precision is not supported, CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE must return 0."
        // not supported
        return returnValue<cl_uint>(0, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF:
        //"If the cl_khr_fp16 extension is not supported, CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF must return 0."
    case CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF:
        //"If the cl_khr_fp16 extension is not supported, CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF must return 0."
        // not supported
        return returnValue<cl_uint>(0, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_CLOCK_FREQUENCY:
    {
        //"Maximum configured clock frequency of the device in MHz."
        QueryMessage<MailboxTag::GET_MAX_CLOCK_RATE> msg({static_cast<uint32_t>(VC4Clock::V3D)});
        if(!mailbox()->readMailboxMessage(msg))
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Error reading mailbox-info V3D max clock rate!");
        return returnValue<cl_uint>(msg.getContent(1) / 1000000 /* clock rate is in Hz -> MHz */, param_value_size,
            param_value, param_value_size_ret);
    }
    case CL_DEVICE_ADDRESS_BITS:
        //"The default compute device address space size specified as an unsigned integer value in bits.
        // Currently supported values are 32 or 64 bits."
        // 32-bit machine
        return returnValue<cl_uint>(32, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_MEM_ALLOC_SIZE:
        //"Max size of memory object allocation in bytes.  The minimum value is max (1/4th of CL_DEVICE_GLOBAL_MEM_SIZE,
        // 1 MB)"
        return returnValue<cl_ulong>(
            mailbox()->getTotalGPUMemory(), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE_SUPPORT:
        //"Is CL_TRUE if images are supported by the OpenCL device and CL_FALSE otherwise."
#ifdef IMAGE_SUPPORT
        return returnValue<cl_bool>(CL_TRUE, param_value_size, param_value, param_value_size_ret);
#else
        return returnValue<cl_bool>(CL_FALSE, param_value_size, param_value, param_value_size_ret);
#endif
    case CL_DEVICE_MAX_READ_IMAGE_ARGS:
        //"Max number of simultaneous image objects that can be read by a kernel."
        return returnValue<cl_uint>(
            kernel_config::MAX_PARAMETER_COUNT / 2, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_WRITE_IMAGE_ARGS:
        //"Max number of simultaneous image objects that can be written to by a kernel."
        return returnValue<cl_uint>(
            kernel_config::MAX_PARAMETER_COUNT / 2, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE2D_MAX_WIDTH:
        //"Max width of 2D image in pixels.  The minimum value is 2048 [...]"
        return returnValue<size_t>(
            kernel_config::MAX_IMAGE_DIMENSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE2D_MAX_HEIGHT:
        //"Max width of 2D image in pixels.  The minimum value is 2048 [...]"
        return returnValue<size_t>(
            kernel_config::MAX_IMAGE_DIMENSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE3D_MAX_WIDTH:
        //"Max width of 2D image in pixels.  The minimum value is 0 [...]"
        return returnValue<size_t>(
            kernel_config::MAX_IMAGE_DIMENSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE3D_MAX_HEIGHT:
        //"Max width of 2D image in pixels.  The minimum value is 0 [...]"
        return returnValue<size_t>(
            kernel_config::MAX_IMAGE_DIMENSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE3D_MAX_DEPTH:
        //"Max width of 2D image in pixels.  The minimum value is 0 [...]"
        return returnValue<size_t>(
            kernel_config::MAX_IMAGE_DIMENSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE_MAX_BUFFER_SIZE:
        //"Max width of 2D image in pixels.  The minimum value is 2048 [...]"
        return returnValue<size_t>(
            kernel_config::MAX_IMAGE_DIMENSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IMAGE_MAX_ARRAY_SIZE:
        //"Max width of 2D image in pixels.  The minimum value is 256 [...]"
        return returnValue<size_t>(
            kernel_config::MAX_IMAGE_DIMENSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_SAMPLERS:
        //"Maximum number of samplers that can be used in a kernel."
        return returnValue<cl_uint>(
            kernel_config::MAX_PARAMETER_COUNT / 2, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_PARAMETER_SIZE:
        //"Max size in bytes of the arguments that can be passed to a kernel. The minimum value is 1024 (256 for
        // EMBEDDED PROFILE)."
        // TODO this is not correct, e.g. for literal vector parameters, which only use a single register/parameter (but
        // more than 4 byte each)
        return returnValue<size_t>(kernel_config::MAX_PARAMETER_COUNT * sizeof(uint32_t) /* 32-bit integers */,
            param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MEM_BASE_ADDR_ALIGN:
        // OpenCL 1.0: "Describes the alignment in bits of the base address of any allocated memory object."
        // OpenCL 1.2: "The minimum value is the size (in bits) of the largest OpenCL built-in data type supported by
        // the device."  OpenCL 2.0: "Alignment requirement (in bits) for sub-buffer offsets. The minimum value is the
        // size (in bits) of the largest OpenCL built-in data type supported by the device
        // (long16 in FULL profile, long16 or int16 in EMBEDDED profile)"
        return returnValue<cl_uint>(8 * sizeof(cl_int16), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MIN_DATA_TYPE_ALIGN_SIZE:
        //"The smallest alignment in bytes which can be used for any data type."
        //"The minimum value is the size (in bytes) of the largest OpenCL built-in data type supported by the device
        //(int16 in EMBEDDED profile)."
        //-> deprecated in OpenCL 1.2
        return returnValue<cl_uint>(sizeof(cl_int16), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_SINGLE_FP_CONFIG:
        //"Describes single precision floating-point capability of the device.  This is a bit-field[...]
        // The mandated minimum floating-point capability is: CL_FP_ROUND_TO_NEAREST | CL_FP_INF_NAN."
        // For EMBEDDED PROFILE:
        //"The mandated minimum single precision floating-point capability [...] is
        //  CL_FP_ROUND_TO_ZERO or CL_FP_ROUND_TO_NEAREST"
        return returnValue<cl_device_fp_config>(
            device_config::FLOATING_POINT_CONFIG, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_DOUBLE_FP_CONFIG:
        //"Double precision is an optional feature so the mandated minimum double precision floating-point capability is
        // 0."
    case CL_DEVICE_HALF_FP_CONFIG:
        // cl_khr_fp16 specifies:
        //"Describes half precision floating-point capability of the OpenCL device. [...] The required minimum half
        // precision
        // floating-point capability as implemented by this extension is CL_FP_ROUND_TO_ZERO or CL_FP_ROUND_TO_NEAREST |
        // CL_FP_INF_NAN."
        // not supported
        return returnValue<cl_device_fp_config>(0, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_GLOBAL_MEM_CACHE_TYPE:
        //"Type of global memory cache supported."
        return returnValue<cl_device_mem_cache_type>(
            CL_READ_WRITE_CACHE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE:
        //"Size of global memory cache line in bytes."
        return returnValue<cl_uint>(
            device_config::CACHE_LINE_SIZE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_GLOBAL_MEM_CACHE_SIZE:
        //"Size of global memory cache in bytes."
        return returnValue<cl_ulong>(device_config::CACHE_SIZE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_GLOBAL_MEM_SIZE:
        //"Size of global device memory in bytes."
        return returnValue<cl_ulong>(
            mailbox()->getTotalGPUMemory(), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE:
        //"Max size in bytes of a constant buffer allocation.  The minimum value is 64 KB (1KB for EMBEDDED PROFILE)"
        return returnValue<cl_ulong>(
            mailbox()->getTotalGPUMemory(), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_MAX_CONSTANT_ARGS:
        //"Max number of arguments declared with the __constant qualifier in a kernel.  The minimum value is 8 (4 for
        // EMBEDDED PROFILE)"
        // Return less than our hard parameter limit to make the OpenCL-CTS test pass. We still support way more
        // __constant parameters than required.
        return returnValue<cl_uint>(
            kernel_config::MAX_PARAMETER_COUNT / 2, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_LOCAL_MEM_TYPE:
        //"Type of local memory supported.  This can be set to CL_LOCAL implying dedicated local memory storage such as
        // SRAM, or CL_GLOBAL."  memory is always global
        return returnValue<cl_device_local_mem_type>(CL_GLOBAL, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_LOCAL_MEM_SIZE:
        //"Size of local memory arena in bytes.  The minimum value is 32 KB (1KB for EMBEDDED PROFILE)"
        return returnValue<cl_ulong>(
            mailbox()->getTotalGPUMemory(), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_ERROR_CORRECTION_SUPPORT:
        // Is CL_TRUE if the device implements error correction for all accesses to compute device memory (global and
        // constant)"
        return returnValue<cl_bool>(CL_FALSE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_HOST_UNIFIED_MEMORY:
        //"Is CL_TRUE if the device and the host have a unified memory subsystem and is CL_FALSE otherwise."
        return returnValue<cl_bool>(CL_TRUE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PROFILING_TIMER_RESOLUTION:
        //"Describes the resolution of device timer. This is measured in nanoseconds."
        return returnValue<size_t>(clockResolution, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_ENDIAN_LITTLE:
        //"Is CL_TRUE if the OpenCL device is a little endian device and CL_FALSE otherwise."
        // by default, ARM seem to be little-endian, no specific information found on the GPU
        return returnValue<cl_bool>(CL_TRUE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_AVAILABLE:
        //"Is CL_TRUE if the device is available and CL_FALSE if the device is not available."
        return returnValue<cl_bool>(CL_TRUE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_COMPILER_AVAILABLE:
        //"Is CL_FALSE if the implementation does not have a compiler available to compile the program source."
#if defined(HAS_COMPILER) && HAS_COMPILER == 1
        return returnValue<cl_bool>(CL_TRUE, param_value_size, param_value, param_value_size_ret);
#else
        return returnValue<cl_bool>(CL_FALSE, param_value_size, param_value, param_value_size_ret);
#endif
    case CL_DEVICE_LINKER_AVAILABLE:
        //"Is CL_FALSE if the implementation does not have a linker available."
#if defined(HAS_COMPILER) && HAS_COMPILER == 1
        return returnValue<cl_bool>(
            vc4c::Precompiler::isLinkerAvailable(), param_value_size, param_value, param_value_size_ret);
#else
        return returnValue<cl_bool>(CL_FALSE, param_value_size, param_value, param_value_size_ret);
#endif
    case CL_DEVICE_EXECUTION_CAPABILITIES:
        //"Describes the execution capabilities of the device.  This is a bit-field[...]
        // The mandated minimum capability is: CL_EXEC_KERNEL."
        // only supports OpenCL kernels
        return returnValue<cl_device_exec_capabilities>(
            CL_EXEC_KERNEL, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_QUEUE_PROPERTIES:
        //"Describes the command-queue properties supported by the device.  This is a bit-field [...]
        // The mandated minimum capability is: CL_QUEUE_PROFILING_ENABLE."
        return returnValue<cl_command_queue_properties>(
            CL_QUEUE_PROFILING_ENABLE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_BUILT_IN_KERNELS:
        //"A semi-colon separated list of built-in kernels supported by the device.
        // An empty string is returned if no built-in kernels are supported by the device."
        return returnString("", param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PLATFORM:
        //"The platform associated with this device."
        return returnValue<cl_platform_id>(
            Platform::getVC4CLPlatform().toBase(), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_NAME:
        return returnString(device_config::NAME, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_VENDOR:
        return returnString(device_config::VENDOR, param_value_size, param_value, param_value_size_ret);
    case CL_DRIVER_VERSION:
        //"OpenCL software driver version string in the form major_number.minor_number"
        return returnString(platform_config::VC4CL_VERSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PROFILE:
        //"OpenCL profile string.  Returns the profile name supported by the device"
        return returnString(platform_config::PROFILE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_VERSION:
        //"OpenCL version string.  Returns the OpenCL version supported by the device.
        // This version string has the following format:
        // OpenCL<space><major_version.minor_version><space><vendor-specific information>"
        return returnString(platform_config::VERSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_OPENCL_C_VERSION:
        //"OpenCL C version string.  Returns the highest OpenCL C version supported by the compiler.
        // This version string has the following format:
        // OpenCL<space>C<space><major_version.minor_version><space><vendor-specific information>"
        return returnString(device_config::COMPILER_VERSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_EXTENSIONS:
        // Returns a space separated list of extension names (the extension names themselves do not contain any spaces)
        // supported by the device.
        // The following approved Khronos extension names must be returned by all device that support OpenCL C 1.2:
        // cl_khr_global_int32_base_atomics, cl_khr_global_int32_extended_atomics, cl_khr_local_int32_base_atomics,
        // cl_khr_local_int32_extended_atomics, cl_khr_byte_addressable_store"
        // TODO are all platform extensions always also device extensions?? Or is my associated wrong?
        // OpenCL CTS expects e.g. "cl_khr_spir" and "cl_khr_icd" to be device extensions (see
        // https://github.com/KhronosGroup/OpenCL-CTS/blob/cl12_trunk/test_conformance/compiler/test_compiler_defines_for_extensions.cpp)
        return returnString(joinStrings(device_config::EXTENSIONS, [](const Extension& e) { return e.name; }) + " " +
                joinStrings(platform_config::EXTENSIONS, [](const Extension& e) { return e.name; }),
            param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PRINTF_BUFFER_SIZE:
        //"Maximum size of the internal buffer that holds the output of printf calls from a kernel.
        // The minimum value for the FULL profile is 1 MB (1KB for EMBEDDED PROFILE)."
        // TODO printf support (OpenCL 1.2, page 284) -> write to VPM (special area, with index for current position)
        // and print from host..  but how to synchronize index, etc. ??
        return returnValue<size_t>(0, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PREFERRED_INTEROP_USER_SYNC:
        //"Is CL_TRUE if the device's preference is for the user to be responsible for synchronization,
        // when sharing memory objects between OpenCL and other APIs [...]"
        // we do not offer any synchronization technique (for memory)
        return returnValue<cl_bool>(CL_TRUE, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PARENT_DEVICE:
        //"Returns the cl_device_id of the parent device to which this sub-device belongs.
        // If device is a root-level device, a NULL value is returned."
        return returnValue<cl_device_id>(nullptr, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PARTITION_MAX_SUB_DEVICES:
        //"Returns the maximum number of sub-devices that can be created when a device is partitioned."
        // device-partitioning is not supported, for simplicity
        return returnValue<cl_uint>(0, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PARTITION_PROPERTIES:
        //"Returns the list of partition types supported by device.
        // If the device cannot be partitioned, a value of 0 will be returned."
        // for simplicity, disallow partitioning
        return returnValue(
            &dummy, sizeof(cl_device_partition_property), 1, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PARTITION_AFFINITY_DOMAIN:
        //"Returns the list of supported affinity domains for partitioning the device using
        // CL_DEVICE_PARTITION_BY_AFFINITY_DOMAIN.
        // If the device does not support any affinity domains, a value of 0 will be returned."
        // not supported
        return returnValue<cl_device_affinity_domain>(0, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_PARTITION_TYPE:
        //"[...] return a property value of 0 [...]"
        // not supported
        return returnValue(
            &dummy, sizeof(cl_device_partition_property), 1, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_REFERENCE_COUNT:
        //"Returns the device reference count.  If the device is a root-level device, a reference count of one is
        // returned"
        return returnValue<cl_uint>(1, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_SPIR_VERSIONS:
        // OpenCL 1.2 Extension specification, page 135:
        //"A space separated list of SPIR versions supported by the device. For example returning “1.2 2.0” in this
        // query implies that SPIR version 1.2 and 2.0 are supported by the implementation."  both supported LLVMs
        // support
        // SPIR 1.2
        return returnString(SPIR_VERSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_IL_VERSION_KHR:
        // OpenCL 1.2 Extension specification, page 154:
        //"The intermediate languages that can be supported by clCreateProgramWithILKHR for this device.
        // Set to a space separated list of IL version strings of the form <IL_Prefix>_<Major_version>.<Minor_version>.
        // "SPIR-V" is a required IL prefix [...]"
        return returnString(
            SPIRV_VERSION " " SPIR_PREFIX SPIR_VERSION, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_CORE_TEMPERATURE_ALTERA:
    {
        // cl_altera_device_temperature -
        // https://www.khronos.org/registry/OpenCL/extensions/altera/cl_altera_device_temperature.txt "The core die
        // temperature of the device, in degrees Celsius. If the device does not support the query, the result will
        // default to 0."
        QueryMessage<MailboxTag::GET_TEMPERATURE> msg({0});
        //"Return the temperature of the SoC in thousandths of a degree C. id should be zero."
        if(!mailbox()->readMailboxMessage(msg))
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Error reading mailbox-info device temperature!");
        return returnValue<cl_int>(msg.getContent(1) / 1000, param_value_size, param_value, param_value_size_ret);
    }
    case CL_DEVICE_COMPUTE_UNITS_BITFIELD_ARM:
        // cl_arm_core_id - https://www.khronos.org/registry/OpenCL/extensions/arm/cl_arm_get_core_id.txt
        // "returns a bitfield where each bit set represents the presence of compute unit whose ID is the bit position."
        return returnValue<cl_ulong>(1, param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_NUMERIC_VERSION_KHR:
        // cl_khr_extended_versioning
        // "Returns detailed (major, minor, patch) numeric version information. The major and minor version numbers
        // returned must match those returned via `CL_DEVICE_VERSION`."
        return returnValue<cl_version_khr>(
            CL_MAKE_VERSION_KHR(1, 2, 0), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_OPENCL_C_NUMERIC_VERSION_KHR:
        // cl_khr_extended_versioning
        // "Returns detailed (major, minor, patch) numeric version information. The major and minor version numbers
        // returned must match those returned via `CL_DEVICE_OPENCL_C_VERSION`."
        return returnValue<cl_version_khr>(
            CL_MAKE_VERSION_KHR(1, 2, 0), param_value_size, param_value, param_value_size_ret);
    case CL_DEVICE_EXTENSIONS_WITH_VERSION_KHR:
    {
        // cl_khr_extended_versioning
        // "Returns an array of description (name and version) structures. The same extension name must not be reported
        // more than once. The list of extensions reported must match the list reported via `CL_DEVICE_EXTENSIONS`."
        // TODO see CL_DEVICE_EXTENSIONS for the discussion whether all platform extensions also have to be listed for
        // the device
        auto allExtensions = device_config::EXTENSIONS;
        allExtensions.insert(
            allExtensions.end(), platform_config::EXTENSIONS.begin(), platform_config::EXTENSIONS.end());
        return returnExtensions(allExtensions, param_value_size, param_value, param_value_size_ret);
    }
    case CL_DEVICE_ILS_WITH_VERSION_KHR:
    {
        // cl_khr_extended_versioning
        // "Returns an array of descriptions (name and version) for all supported Intermediate Languages. Intermediate
        // Languages with the same name may be reported more than once but each name and major/minor version combination
        // may only be reported once. The list of intermediate languages reported must match the list reported via
        // `CL_DEVICE_IL_VERSION`."
        std::vector<cl_name_version_khr> data{
            cl_name_version_khr{CL_MAKE_VERSION_KHR(SPIR_VERSION_MAJOR, SPIR_VERSION_MINOR, 0), "SPIR"},
            cl_name_version_khr{CL_MAKE_VERSION_KHR(SPIRV_VERSION_MAJOR, SPIRV_VERSION_MINOR, 0), "SPIR-V"}};
        return returnValue(
            data.data(), sizeof(cl_name_version_khr), data.size(), param_value_size, param_value, param_value_size_ret);
    }
    case CL_DEVICE_BUILT_IN_KERNELS_WITH_VERSION_KHR:
        // cl_khr_extended_versioning
        // "Returns an array of descriptions for the built-in kernels supported by the device. Each built-in kernel may
        // only be reported once. The list of reported kernels must match the list returned via
        // `CL_DEVICE_BUILT_IN_KERNELS`."
        return returnValue(
            nullptr, sizeof(cl_name_version_khr), 0, param_value_size, param_value, param_value_size_ret);
    default:
        // invalid parameter-name
        return returnError(
            CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_device_info value %u", param_name));
    }

    return CL_SUCCESS;
}

/*!
 * Table 4.2
 *  CL_DEVICE_TYPE_CPU			An OpenCL device that is the host processor.  The host processor runs the OpenCL
 * implementations and is a single or multi-core CPU.
 *  CL_DEVICE_TYPE_GPU			An OpenCL device that is a GPU.  By this we mean that the device can also be used to
 * accelerate
 * a 3D API such as OpenGL or DirectX. CL_DEVICE_TYPE_ACCELERATOR	Dedicated OpenCL accelerators (for example the IBM
 * CELL Blade).  These devices communicate with the host processor using a peripheral interconnect such as PCIe.
 *  CL_DEVICE_TYPE_CUSTOM		Dedicated accelerators that do not support programs written in OpenCL C.
 *  CL_DEVICE_TYPE_DEFAULT		The default OpenCL device in the system. The default device cannot be a
 * CL_DEVICE_TYPE_CUSTOM device. CL_DEVICE_TYPE_ALL			All OpenCL devices available in the system except
 * CL_DEVICE_TYPE_CUSTOM devices.
 */

/*!
 * OpenCL 1.2 specification, pages 35+:
 *  The list of devices available on a platform can be obtained using the following function.
 *
 *  \param platform refers to the platform ID returned by clGetPlatformIDs or can be NULL. If platform is NULL, the
 * behavior is implementation-defined.
 *
 *  \param device_type is a bitfield that identifies the type of OpenCL device.  The device_type can be used to query
 * specific OpenCL devices or all OpenCL devices available. The valid values for device_type are specified in table 4.2.
 *
 *  \param num_entries is the number of cl_device_id entries that can be added to devices. If devices is not NULL, the
 * num_entries must be greater than zero.
 *
 *  \param devices returns a list of OpenCL devices found.  The cl_device_id values returned in devices can be used to
 * identify a specific OpenCL device. If devices argument is NULL, this argument is ignored.  The number of OpenCL
 * devices returned is the minimum of the value specified by num_entries or the number of OpenCL devices whose type
 * matches device_type.
 *
 *  \param num_devices returns the number of OpenCL devices available that match device_type.  If num_devices is NULL,
 * this argument is ignored.
 *
 *  \return clGetDeviceIDs returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_PLATFORM if platform is not a valid platform.
 *  - CL_INVALID_DEVICE_TYPE if device_type is not a valid value.
 *  - CL_INVALID_VALUE if num_entries is equal to zero and devices is not NULL or if both num_devices and devices are
 * NULL.
 *  - CL_DEVICE_NOT_FOUND if no OpenCL devices that matched device_type were found.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetDeviceIDs)(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries,
    cl_device_id* devices, cl_uint* num_devices)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetDeviceIDs, "cl_platform_id", platform, "cl_device_type", device_type, "cl_uint",
        num_entries, "cl_device_id*", devices, "cl_uint*", num_devices);
    CHECK_PLATFORM(platform)

    if(devices == nullptr && num_devices == nullptr)
        // can't return anything
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "No output parameter set!");

    if(num_entries == 0)
    {
        if(devices != nullptr)
        {
            // can't fetch 0 devices
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Cannot retrieve 0 devices!");
        }
    }

    cl_uint num_found = 0;

    if(hasFlag<cl_device_type>(device_type, CL_DEVICE_TYPE_DEFAULT) ||
        hasFlag<cl_device_type>(device_type, CL_DEVICE_TYPE_GPU))
    {
        // default device queried -> GPU
        if(devices != nullptr)
            devices[num_found] = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
        num_found = 1;
    }
    else
        return returnError(CL_DEVICE_NOT_FOUND, __FILE__, __LINE__,
            buildString("No device for the given criteria: platform %p, type: %d!", platform, device_type));

    if(num_devices != nullptr)
    {
        *num_devices = num_found;
    }

    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 37+:
 *  Gets specific information about an OpenCL device. device may be a device returned by clGetDeviceIDs or a sub-device
 * created by clCreateSubDevices. If device is a sub-device, the specific information for the sub-device will be
 * returned. The information that can be queried using clGetDeviceInfo is specified in table 4.3.
 *
 *  \param device is a device returned by clGetDeviceIDs.
 *
 *  \param param_name is an enumeration constant that identifies the device information being queried.  It can be one of
 * the following values as specified in table 4.3.
 *
 *  \param param_value is a pointer to memory location where appropriate values for a given param_name as specified in
 * table 4.3 will be returned.  If param_value is NULL, it is ignored.
 *
 *  \param param_value_size specifies the size in bytes of memory pointed to by param_value.  This size in bytes must be
 * >= size of return type specified in table 4.3.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data being queried by param_value.  If
 * param_value_size_ret is NULL, it is ignored.
 *
 *  The device queries described in table 4.3 should return the same information for a root-level device i.e. a device
 * returned by clGetDeviceIDs and any sub-devices created from this device except for the following queries:
 *  CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, CL_DEVICE_BUILT_IN_KERNELS, CL_DEVICE_PARENT_DEVICE, CL_DEVICE_PARTITION_TYPE,
 * CL_DEVICE_REFERENCE_COUNT
 *
 *  \return clGetDeviceInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_DEVICE if device is not valid.
 *  - CL_INVALID_VALUE if param_name is not one of the supported values or if size in bytes specified by
 * param_value_size is < size of return type as specified in table 4.3 and param_value is not a NULL value or if
 * param_name is a value that is available as an extension and the corresponding extension is not supported by the
 * device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 */
cl_int VC4CL_FUNC(clGetDeviceInfo)(cl_device_id device, cl_device_info param_name, size_t param_value_size,
    void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetDeviceInfo, "cl_device_id", device, "cl_device_info", param_name, "size_t",
        param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_DEVICE(toType<Device>(device))
    return toType<Device>(device)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

/*!
 * OpenCL 1.2 specification, pages 50+:
 *  Creates an array of sub-devices that each reference a non-intersecting set of compute units within in_device,
 * according to a partition scheme given by properties. The output sub-devices may be used in every way that the root
 * (or parent) device can be used, including creating contexts, building programs, further calls to clCreateSubDevices
 *  and creating command-queues.  When a command-queue is created against a sub-device, the commands enqueued on the
 * queue are executed only on the sub-device.
 *
 *  \param in_device is the device to be partitioned.
 *
 *  \param properties specifies how in_device is to be partition described by a partition name and its corresponding
 * value. Each partition name is immediately followed by the corresponding desired value. The list is terminated with 0.
 * The list of supported partitioning schemes is described in table 4.4. Only one of the listed partitioning schemes
 * can be specified in properties.
 *
 *  \param num_devices is the size of memory pointed to by out_devices specified as the number of cl_device_id entries.
 *
 *  \param out_devices is the buffer where the OpenCL sub-devices will be returned. If out_devices is NULL, this
 * argument is ignored. If out_devices is not NULL, num_devices must be greater than or equal to the number of
 * sub-devices that device may be partitioned into according to the partitioning scheme specified in properties.
 *
 *  \param num_devices_ret returns the number of sub-devices that device may be partitioned into according to the
 * partitioning scheme specified in properties. If num_devices_ret is NULL, it is ignored.
 *
 *  \return clCreateSubDevices returns CL_SUCCESS if the partition is created successfully. Otherwise, it returns a NULL
 * value with the following error values returned in errcode_ret:
 *  - CL_INVALID_DEVICE if in_device is not valid.
 *  - CL_INVALID_VALUE if values specified in properties are not valid or if values specified in properties are valid
 * but not supported by the device.
 *  - CL_INVALID_VALUE if out_devices is not NULL and num_devices is less than the number of sub-devices created by the
 * partition scheme.
 *  - CL_DEVICE_PARTITION_FAILED if the partition name is supported by the implementation but in_device could not be
 * further partitioned.
 *  - CL_INVALID_DEVICE_PARTITION_COUNT if the partition name specified in properties is CL_DEVICE_PARTITION_BY_COUNTS
 * and the number of sub-devices requested exceeds CL_DEVICE_PARTITION_MAX_SUB_DEVICES or the total number of compute
 * units requested exceeds CL_DEVICE_PARTITION_MAX_COMPUTE_UNITS for in_device, or the number of compute units
 * requested for one or more sub-devices is less than zero or the number of sub-devices requested exceeds
 * CL_DEVICE_PARTITION_MAX_COMPUTE_UNITS for in_device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clCreateSubDevices)(cl_device_id in_device, const cl_device_partition_property* properties,
    cl_uint num_devices, cl_device_id* out_devices, cl_uint* num_devices_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clCreateSubDevices, "cl_device_id", in_device, "const cl_device_partition_property*",
        properties, "cl_uint", num_devices, "cl_device_id*", out_devices, "cl_uint*", num_devices_ret);
    CHECK_DEVICE(toType<Device>(in_device))
    // is not supported

    //"[...] valid but not supported by the device"
    return CL_INVALID_VALUE;
}

/*!
 * OpenCL 1.2 specification, page 53:
 *  Increments the device reference count if device is a valid sub-device created by a call to clCreateSubDevices.
 * If device is a root level device i.e. a cl_device_id returned by clGetDeviceIDs, the device reference count remains
 * unchanged.
 *
 *  \return clRetainDevice returns CL_SUCCESS if the function is executed successfully or the device is a root-level
 * device. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_DEVICE if device is not a valid device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clRetainDevice)(cl_device_id device)
{
    VC4CL_PRINT_API_CALL("cl_int", clRetainDevice, "cl_device_id", device);
    CHECK_DEVICE(toType<Device>(device))
    //"[...] the device is a root-level device [...], the device reference count remains unchanged"
    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 53+:
 *  Decrements the device reference count if device is a valid sub-device created by a call to clCreateSubDevices.
 * If device is a root level device i.e. a cl_device_id returned by clGetDeviceIDs, the device reference count remains
 * unchanged.
 *
 *  \return clReleaseDevice returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_DEVICE if device is not a valid device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  After the device reference count becomes zero and all the objects attached to device (such as command-queues) are
 * released, the device object is deleted.
 */
cl_int VC4CL_FUNC(clReleaseDevice)(cl_device_id device)
{
    VC4CL_PRINT_API_CALL("cl_int", clReleaseDevice, "cl_device_id", device);
    CHECK_DEVICE(toType<Device>(device))
    //"[...] the device is a root-level device [...], the device reference count remains unchanged"
    return CL_SUCCESS;
}
