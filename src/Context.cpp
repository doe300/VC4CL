/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#include "Context.h"

#include "extensions.h"

using namespace vc4cl;

Context::Context(const Device* device, const bool userSync, cl_context_properties memoryToZeroOut,
    const Platform* platform, const ContextProperty explicitProperties, const ContextCallback callback,
    void* userData) :
    device(device),
    userSync(userSync), platform(platform), explicitProperties(explicitProperties), memoryToInitialize(memoryToZeroOut),
    callback(callback), userData(userData)
{
}

Context::~Context() noexcept = default;

cl_int Context::getInfo(
    cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    size_t propertiesSize = 0;
    std::array<cl_context_properties, 7> props{};
    // this makes sure, only the explicit set properties are returned
    if(explicitProperties & ContextProperty::PLATFORM)
    {
        props.at(propertiesSize) = CL_CONTEXT_PLATFORM;
        props.at(propertiesSize + 1) = reinterpret_cast<cl_context_properties>(platform->toBase());
        propertiesSize += 2;
    }
    if(explicitProperties & ContextProperty::USER_SYNCHRONISATION)
    {
        props.at(propertiesSize) = CL_CONTEXT_INTEROP_USER_SYNC;
        props.at(propertiesSize + 1) = userSync ? CL_TRUE : CL_FALSE;
        propertiesSize += 2;
    }
    if(explicitProperties & ContextProperty::INITIALIZE_MEMORY)
    {
        props.at(propertiesSize) = CL_CONTEXT_MEMORY_INITIALIZE_KHR;
        props.at(propertiesSize + 1) = memoryToInitialize;
        propertiesSize += 2;
    }
    if(explicitProperties != ContextProperty::NONE)
    {
        // list needs to be terminated with 0
        props.at(propertiesSize) = static_cast<cl_context_properties>(0);
        propertiesSize += 1;
    }

    switch(param_name)
    {
    case CL_CONTEXT_REFERENCE_COUNT:
        //"Return the context reference count."
        return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
    case CL_CONTEXT_NUM_DEVICES:
        //"Return the number of devices in context."
        return returnValue<cl_uint>(1, param_value_size, param_value, param_value_size_ret);
    case CL_CONTEXT_DEVICES:
        //"Return the list of devices in context."
        return returnValue<cl_device_id>(
            const_cast<cl_device_id>(device->toBase()), param_value_size, param_value, param_value_size_ret);
    case CL_CONTEXT_PROPERTIES:
        //"Return the properties argument specified in clCreateContext or clCreateContextFromType."
        return returnValue(props.data(), sizeof(cl_context_properties), propertiesSize, param_value_size, param_value,
            param_value_size_ret);
    default:
        return returnError(
            CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_context_info value %u", param_name));
    }
}

void Context::fireCallback(const std::string& errorInfo, const void* privateInfo, size_t cb)
{
    if(callback != nullptr)
    {
        callback(errorInfo.data(), privateInfo, cb, userData);
    }
}

bool Context::initializeMemoryToZero(cl_context_properties memoryType) const
{
    return (explicitProperties & ContextProperty::INITIALIZE_MEMORY) && (memoryToInitialize & memoryType) != 0;
}

const Context* HasContext::context() const
{
    return c.get();
}

Context* HasContext::context()
{
    return c.get();
}

/*!
 * OpenCL 1.2 specification, page 55+:
 *  Creates an OpenCL context.  An OpenCL context is created with one or more devices. Contexts are used by the OpenCL
 * runtime for managing objects such as command-queues, memory, program and kernel objects and for executing kernels on
 * one or more devices specified in the context.
 *
 *  \param properties specifies a list of context property names and their corresponding values.  Each property name is
 * immediately followed by the corresponding desired value. The list is terminated with 0. The list of supported
 * properties is described in table 4.5. properties can be NULL in which case the platform that is selected is
 * implementation-defined.
 *
 *  \param num_devices is the number of devices specified in the devices argument.
 *
 *  \param devices is a pointer to a list of unique devices returned by clGetDeviceIDs or sub-devices created by
 * clCreateSubDevices for a platform.
 *
 *  \param pfn_notify is a callback function that can be registered by the application.  This callback function will be
 * used by the OpenCL implementation to report information on errors during context creation as well as errors that
 * occur at runtime in this context.  This callback function may be called asynchronously by the OpenCL implementation.
 *  It is the application’s responsibility to ensure that the callback function is thread-safe.  The parameters to this
 * callback function are: errinfo is a pointer to an error string. private_info and cb represent a pointer to binary
 * data that is returned by the OpenCL implementation that can be used to log additional information helpful in
 * debugging the error. user_data is a pointer to user supplied data.
 *
 *  If pfn_notify is NULL, no callback function is registered.
 *
 *  NOTE: There are a number of cases where error notifications need to be delivered due to an error that occurs outside
 * a context. Such notifications may not be delivered through the pfn_notify callback. Where these notifications go is
 * implementation-defined.
 *
 *  \param user_data will be passed as the user_data argument when pfn_notify is called. user_data can be NULL.
 *
 *  \param errcode_ret will return an appropriate error code.  If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateContext returns a valid non-zero context and errcode_ret is set to CL_SUCCESS if the context is
 * created successfully. Otherwise, it returns a NULL value with the following error values returned in errcode_ret:
 *  - CL_INVALID_PLATFORM if properties is NULL and no platform could be selected or if platform value specified in
 * properties is not a valid platform.
 *  - CL_INVALID_PROPERTY if context property name in properties is not a supported property name, if the value
 * specified for a supported property name is not valid, or if the same property name is specified more than once.
 *  - CL_INVALID_VALUE if devices is NULL.
 *  - CL_INVALID_VALUE if num_devices is equal to zero.
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL.
 *  - CL_INVALID_DEVICE if devices contains an invalid device.
 */
cl_context VC4CL_FUNC(clCreateContext)(const cl_context_properties* properties, cl_uint num_devices,
    const cl_device_id* devices,
    void(CL_CALLBACK* pfn_notify)(const char* errinfo, const void* private_info, size_t cb, void* user_data),
    void* user_data, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_context", clCreateContext, "const cl_context_properties*", properties, "cl_uint",
        num_devices, "const cl_device_id*", devices,
        "void(CL_CALLBACK*)(const char* errinfo, const void* private_info, size_t cb, void* user_data)", &pfn_notify,
        "void*", user_data, "cl_int*", errcode_ret);
    ContextProperty explicitProperties = ContextProperty::NONE;
    cl_platform_id platform = Platform::getVC4CLPlatform().toBase();
    bool user_sync = false;
    cl_context_properties memoryToInitialize = 0;

    if(properties != nullptr)
    {
        const cl_context_properties* ptr = properties;
        while(*ptr != 0)
        {
            if(*ptr == CL_CONTEXT_PLATFORM)
            {
                explicitProperties = explicitProperties | ContextProperty::PLATFORM;
                ++ptr;
                platform = reinterpret_cast<cl_platform_id>(*ptr);
                ++ptr;
            }
            else if(*ptr == CL_CONTEXT_INTEROP_USER_SYNC)
            {
                explicitProperties = explicitProperties | ContextProperty::USER_SYNCHRONISATION;
                ++ptr;
                user_sync = *ptr == CL_TRUE;
                ++ptr;
            }
            else if(*ptr == CL_CONTEXT_MEMORY_INITIALIZE_KHR)
            {
                explicitProperties = explicitProperties | ContextProperty::INITIALIZE_MEMORY;
                ++ptr;
                memoryToInitialize = *ptr;
                ++ptr;
            }
            else
                return returnError<cl_context>(CL_INVALID_PROPERTY, errcode_ret, __FILE__, __LINE__,
                    buildString("Invalid cl_context_properties value %d!", *ptr));
        }
    }

    if(platform != nullptr && platform != Platform::getVC4CLPlatform().toBase())
        return returnError<cl_context>(
            CL_INVALID_PLATFORM, errcode_ret, __FILE__, __LINE__, "Platform is not the VC4CL platform!");

    if(num_devices == 0 || devices == nullptr)
        return returnError<cl_context>(CL_INVALID_PROPERTY, errcode_ret, __FILE__, __LINE__, "No device(s) specified!");

    size_t i = 0;
    for(; i < num_devices; ++i)
    {
        // ignore duplicate devices, so check every device for GPU
        if(devices[i] != Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase())
            return returnError<cl_context>(
                CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__, "Device is not the VC4CL GPU device!");
    }
    cl_device_id device = *devices;

    if(pfn_notify == nullptr && user_data != nullptr)
        return returnError<cl_context>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "User data given, but no callback set!");

    Context* context = newOpenCLObject<Context>(toType<Device>(device), user_sync, memoryToInitialize,
        &Platform::getVC4CLPlatform(), explicitProperties, pfn_notify, user_data);
    CHECK_ALLOCATION_ERROR_CODE(context, errcode_ret, cl_context)
    RETURN_OBJECT(context->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pages 57+:
 *  Creates an OpenCL context from a device type that identifies the specific device(s) to use.
 * Only devices that are returned by clGetDeviceIDs for device_type are used to create the context.  The context does
 * not reference any sub-devices that may have been created from these devices.
 *
 *  \param properties specifies a list of context property names and their corresponding values.  Each property name is
 * immediately followed by the corresponding desired value. The list of supported properties is described in table 4.5.
 * properties can also be NULL in which case the platform that is selected is implementation-defined.
 *
 *  \param device_type is a bit-field that identifies the type of device and is described in table 4.2 in section 4.2.
 *
 *  \param pfn_notify and user_data are described in clCreateContext.
 *
 *  \param errcode_ret will return an appropriate error code.  If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateContextFromType returns a valid non-zero context and errcode_ret is set to CL_SUCCESS if the context
 * is created successfully. Otherwise, it returns a NULL value with the following error values returned in errcode_ret:
 *  - CL_INVALID_PLATFORM if properties is NULL and no platform could be selected or if platform value specified in
 * properties is not a valid platform.
 *  - CL_INVALID_PROPERTY if context property name in properties is not a supported property name, if the value
 * specified for a supported property name is not valid, or if the same property name is specified more than once.
 *  - CL_INVALID_VALUE if pfn_notify is NULL but user_data is not NULL.
 *  - CL_INVALID_DEVICE_TYPE if device_type is not a valid value.
 *  - CL_DEVICE_NOT_AVAILABLE if no devices that match device_type and property values specified in properties are
 * currently available.
 *  - CL_DEVICE_NOT_FOUND if no devices that match device_type and property values specified in properties were found.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_context VC4CL_FUNC(clCreateContextFromType)(const cl_context_properties* properties, cl_device_type device_type,
    void(CL_CALLBACK* pfn_notify)(const char* errinfo, const void* private_info, size_t cb, void* user_data),
    void* user_data, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_context", clCreateContextFromType, "const cl_context_properties*", properties,
        "cl_device_type", device_type,
        "void(CL_CALLBACK*)(const char* errinfo, const void* private_info, size_t cb, void* user_data)", &pfn_notify,
        "void*", user_data, "cl_int*", errcode_ret);
    cl_uint num_devices = 0;
    cl_device_id device;
    cl_int error =
        VC4CL_FUNC(clGetDeviceIDs)(Platform::getVC4CLPlatform().toBase(), device_type, 1, &device, &num_devices);
    if(error != CL_SUCCESS)
        return returnError<cl_context>(error, errcode_ret, __FILE__, __LINE__, "Failed to get device ID!");
    return VC4CL_FUNC(clCreateContext)(properties, num_devices, &device, pfn_notify, user_data, errcode_ret);
}

/*!
 * OpenCL 1.2 specification, page 58:
 *  Increments the context reference count.
 *
 *  \return clRetainContext returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_CONTEXT if context is not a valid OpenCL context.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  clCreateContext and clCreateContextFromType perform an implicit retain. This is very helpful for 3rdparty libraries,
 * which typically get a context passed to them by the application. However, it is possible that the application may
 * delete the context without informing the library. Allowing functions to attach to (i.e. retain) and release a
 * context solves the problem of a context being used by a library no longer being valid.
 */
cl_int VC4CL_FUNC(clRetainContext)(cl_context context)
{
    VC4CL_PRINT_API_CALL("cl_int", clRetainContext, "cl_context", context);
    CHECK_CONTEXT(toType<Context>(context))
    return toType<Context>(context)->retain();
}

/*!
 * OpenCL 1.2 specification, pages 58+:
 *  Decrements the context reference count.
 *
 *  \return clReleaseContext returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_CONTEXT if context is not a valid OpenCL context.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resource s required by the OpenCL implementation on the
 * host.
 *
 *  After the context reference count becomes zero and all the objects attached to context (such as memory objects,
 * command-queues) are released, the context is deleted.
 */
cl_int VC4CL_FUNC(clReleaseContext)(cl_context context)
{
    VC4CL_PRINT_API_CALL("cl_int", clReleaseContext, "cl_context", context);
    CHECK_CONTEXT(toType<Context>(context))
    return toType<Context>(context)->release();
}

/*!
 * OpenCL 1.2 specification, pages 59+:
 *  Can be used to query information a bout a context.
 *
 *  \param context specifies the OpenCL context being queried.
 *
 *  \param param_name is an enumeration constant that specifies the information to query.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned.  If param_value is
 * NULL, it is ignored.
 *
 *  \param param_value_size specifies the size in bytes of memory pointed to by param_value.  This size must be greater
 * than or equal to the size of return type as described in table 4.6.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data being queried by param_value.  If
 * param_value_size_ret is NULL, it is ignored.
 *
 *  The list of supported param_name values and the information returned in param_value by clGetContextInfois described
 * in table 4.6.
 *
 *  \return clGetContextInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if param_name is not one of the supported values or if size in bytes specified by
 * param_value_size is < size of return type as specified in table 4.6 and param_value is not a NULL value.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetContextInfo)(cl_context context, cl_context_info param_name, size_t param_value_size,
    void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetContextInfo, "cl_context", context, "cl_context_info", param_name, "size_t",
        param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_CONTEXT(toType<Context>(context))
    return toType<Context>(context)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
