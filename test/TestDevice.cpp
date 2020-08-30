/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <cstring>

#include "TestDevice.h"
#include "src/common.h"
#include "src/icd_loader.h"
#include "src/Platform.h"
#include "src/V3D.h"

using namespace vc4cl;

TestDevice::TestDevice() : device(nullptr)
{
    TEST_ADD(TestDevice::testGetDeviceIDs);
    TEST_ADD(TestDevice::testGetDeviceInfo);
    TEST_ADD(TestDevice::testCreateSubDevice);
    TEST_ADD(TestDevice::testRetainDevice);
    TEST_ADD(TestDevice::testReleaseDevice);
}

void TestDevice::testGetDeviceIDs()
{
    cl_uint num_devices = 0;
    cl_device_id ids[8];
    cl_int state = VC4CL_FUNC(clGetDeviceIDs)(nullptr, CL_DEVICE_TYPE_ACCELERATOR, 8, ids, &num_devices);
    TEST_ASSERT_EQUALS(CL_DEVICE_NOT_FOUND, state);
    
    state = VC4CL_FUNC(clGetDeviceIDs)(nullptr, CL_DEVICE_TYPE_CPU, 8, ids, &num_devices);
    TEST_ASSERT_EQUALS(CL_DEVICE_NOT_FOUND, state);
    
    state = VC4CL_FUNC(clGetDeviceIDs)(nullptr, CL_DEVICE_TYPE_CUSTOM, 8, ids, &num_devices);
    TEST_ASSERT_EQUALS(CL_DEVICE_NOT_FOUND, state);
    
    state = VC4CL_FUNC(clGetDeviceIDs)(nullptr, CL_DEVICE_TYPE_DEFAULT, 8, ids, &num_devices);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, num_devices);
    TEST_ASSERT_EQUALS(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), ids[0]);
    
    state = VC4CL_FUNC(clGetDeviceIDs)(nullptr, CL_DEVICE_TYPE_GPU, 8, ids, &num_devices);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, num_devices);
    TEST_ASSERT_EQUALS(Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase(), ids[0]);
    
    device = ids[0];
}

void TestDevice::testGetDeviceInfo()
{
    size_t info_size = 0;
    char buffer[1024];
    
    cl_int state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_ADDRESS_BITS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(32u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_AVAILABLE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<cl_bool*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_BUILT_IN_KERNELS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(1u, info_size);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_COMPILER_AVAILABLE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
#ifdef HAS_COMPILER
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<cl_bool*>(buffer));
#else
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_FALSE), *reinterpret_cast<cl_bool*>(buffer));
#endif
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_DOUBLE_FP_CONFIG, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_fp_config), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_device_fp_config*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_ENDIAN_LITTLE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<cl_bool*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_ERROR_CORRECTION_SUPPORT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_FALSE), *reinterpret_cast<cl_bool*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_EXECUTION_CAPABILITIES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_exec_capabilities), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_device_exec_capabilities>(CL_EXEC_KERNEL), *reinterpret_cast<cl_device_exec_capabilities*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_EXTENSIONS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_GLOBAL_MEM_CACHE_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_ulong), info_size);
    TEST_ASSERT_EQUALS(device_config::CACHE_SIZE, *reinterpret_cast<cl_ulong*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_GLOBAL_MEM_CACHE_TYPE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_mem_cache_type), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_device_mem_cache_type>(CL_READ_WRITE_CACHE), *reinterpret_cast<cl_device_mem_cache_type*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(device_config::CACHE_LINE_SIZE, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_HALF_FP_CONFIG, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_fp_config), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_device_fp_config*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_HOST_UNIFIED_MEMORY, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<cl_bool*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_IMAGE_SUPPORT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
#ifdef IMAGE_SUPPORT
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<cl_bool*>(buffer));
#else
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_FALSE), *reinterpret_cast<cl_bool*>(buffer));
#endif
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_LINKER_AVAILABLE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
#ifdef HAS_COMPILER
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<cl_bool*>(buffer));
#else
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_FALSE), *reinterpret_cast<cl_bool*>(buffer));
#endif
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_LOCAL_MEM_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_ulong), info_size);
    TEST_ASSERT((*reinterpret_cast<cl_ulong*>(buffer)) >= 32 * 1024u);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_LOCAL_MEM_TYPE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_local_mem_type), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_device_local_mem_type>(CL_GLOBAL), *reinterpret_cast<cl_device_local_mem_type*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_CLOCK_FREQUENCY, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    //Raspberry Pi 3 runs on 300MHz
	TEST_ASSERT_DELTA(250u, *reinterpret_cast<cl_uint*>(buffer), 50u);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_COMPUTE_UNITS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_CONSTANT_ARGS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT((*reinterpret_cast<cl_uint*>(buffer)) > 8u);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_ulong), info_size);
    TEST_ASSERT((*reinterpret_cast<cl_ulong*>(buffer)) >= 64 * 1024u);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_ulong), info_size);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_PARAMETER_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT((*reinterpret_cast<size_t*>(buffer)) >= 256u);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_WORK_GROUP_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT_EQUALS(V3D::instance()->getSystemInfo(SystemInfo::QPU_COUNT), *reinterpret_cast<size_t*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(3u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MAX_WORK_ITEM_SIZES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(3 * sizeof(size_t), info_size);
    TEST_ASSERT((reinterpret_cast<size_t*>(buffer))[0] >= 1u);
    TEST_ASSERT((reinterpret_cast<size_t*>(buffer))[1] >= 1u);
    TEST_ASSERT((reinterpret_cast<size_t*>(buffer))[2] >= 1u);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_MEM_BASE_ADDR_ALIGN, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(sizeof(cl_int16) * 8 /* size in bits */, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NAME, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NATIVE_VECTOR_WIDTH_CHAR, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NATIVE_VECTOR_WIDTH_SHORT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NATIVE_VECTOR_WIDTH_INT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NATIVE_VECTOR_WIDTH_LONG, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NATIVE_VECTOR_WIDTH_DOUBLE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_NATIVE_VECTOR_WIDTH_HALF, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_OPENCL_C_VERSION, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PARENT_DEVICE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_id), info_size);
    TEST_ASSERT_EQUALS(nullptr, *reinterpret_cast<cl_device_id*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PARTITION_MAX_SUB_DEVICES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PARTITION_PROPERTIES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_partition_property), info_size);
    TEST_ASSERT_EQUALS(0, *reinterpret_cast<cl_device_partition_property*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PARTITION_AFFINITY_DOMAIN, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_affinity_domain), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_device_affinity_domain*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PARTITION_TYPE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_partition_property), info_size);
    TEST_ASSERT_EQUALS(0, *reinterpret_cast<cl_device_partition_property*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PLATFORM, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_platform_id), info_size);
    TEST_ASSERT_EQUALS(Platform::getVC4CLPlatform().toBase(), *reinterpret_cast<cl_platform_id*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_CHAR, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_SHORT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_INT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_LONG, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_FLOAT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(16u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_DOUBLE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_VECTOR_WIDTH_HALF, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PRINTF_BUFFER_SIZE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    TEST_ASSERT_EQUALS(0u, *reinterpret_cast<size_t*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PREFERRED_INTEROP_USER_SYNC, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_bool), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<cl_bool*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PROFILE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT(platform_config::PROFILE.compare(reinterpret_cast<char*>(buffer)) == 0);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_PROFILING_TIMER_RESOLUTION, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(size_t), info_size);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_QUEUE_PROPERTIES, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_command_queue_properties), info_size);
    TEST_ASSERT((*reinterpret_cast<cl_command_queue_properties*>(buffer)) & CL_QUEUE_PROFILING_ENABLE);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_REFERENCE_COUNT, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(1u, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_SINGLE_FP_CONFIG, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_fp_config), info_size);
    TEST_ASSERT((*reinterpret_cast<cl_device_fp_config*>(buffer)) & CL_FP_ROUND_TO_ZERO);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_TYPE, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_device_type), info_size);
    TEST_ASSERT_EQUALS(static_cast<cl_device_type>(CL_DEVICE_TYPE_GPU), *reinterpret_cast<cl_device_type*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_VENDOR, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);

    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_VENDOR_ID, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    TEST_ASSERT_EQUALS(sizeof(cl_uint), info_size);
    TEST_ASSERT_EQUALS(device_config::VENDOR_ID, *reinterpret_cast<cl_uint*>(buffer));
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DEVICE_VERSION, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, CL_DRIVER_VERSION, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
    
    state = VC4CL_FUNC(clGetDeviceInfo)(device, 0xDEADBEAF, 1024, buffer, &info_size);
    TEST_ASSERT_EQUALS(CL_INVALID_VALUE, state);
}

void TestDevice::testCreateSubDevice()
{
    cl_uint num_devices = 0;
    cl_device_id ids[8];
    cl_device_partition_property props[2] = {CL_DEVICE_PARTITION_EQUALLY, 16};
    cl_int state = VC4CL_FUNC(clCreateSubDevices)(device, props, 8, ids, &num_devices);
    TEST_ASSERT(state != CL_SUCCESS);
    TEST_ASSERT_EQUALS(0u, num_devices);
}

void TestDevice::testRetainDevice()
{
    cl_int state = VC4CL_FUNC(clRetainDevice)(device);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}

void TestDevice::testReleaseDevice()
{
    cl_int state = VC4CL_FUNC(clReleaseDevice)(device);
    TEST_ASSERT_EQUALS(CL_SUCCESS, state);
}
