/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Platform.h"

#include "extensions.h"

#include <dlfcn.h>
#include <memory>
#include <pthread.h>
#include <stdexcept>

using namespace vc4cl;

Platform::Platform() : Object()
{
    // we need thread-support, so load the pthread library dynamically (if it is not yet loaded)
    void* handle = dlopen("libpthread.so.0", RTLD_GLOBAL | RTLD_LAZY);
    if(handle == nullptr)
    {
        throw std::runtime_error(std::string("Error loading pthread library: ") + dlerror());
    }
}

Platform::~Platform() noexcept = default;

cl_int Platform::getInfo(
    cl_platform_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) const
{
    switch(param_name)
    {
    case CL_PLATFORM_NAME:
        //"Platform name string."
        return returnString(platform_config::NAME, param_value_size, param_value, param_value_size_ret);
    case CL_PLATFORM_PROFILE:
        //"OpenCL profile string.  Returns the profile name supported by the implementation.
        // EMBEDDED_PROFILE - if the implementation supports the OpenCL embedded profile."
        return returnString(platform_config::PROFILE, param_value_size, param_value, param_value_size_ret);
    case CL_PLATFORM_VENDOR:
        //"Platform vendor string."
        return returnString(platform_config::VENDOR, param_value_size, param_value, param_value_size_ret);
    case CL_PLATFORM_VERSION:
        //"Returns the OpenCL version supported by the implementation. [...]
        // OpenCL<space><major_version.minor_version><space><platform-specific information> The
        // major_version.minor_version value returned will be 1.2"
        return returnString(platform_config::VERSION, param_value_size, param_value, param_value_size_ret);
    case CL_PLATFORM_EXTENSIONS:
        //"Returns a space separated list of extension names [...] supported by the platform."
        return returnString(joinStrings(platform_config::EXTENSIONS, [](const Extension& e) { return e.name; }),
            param_value_size, param_value, param_value_size_ret);
    case CL_PLATFORM_ICD_SUFFIX_KHR:
        // enabled by the cl_khr_icd extension
        //"The function name suffix used to identify extension functions to be directed to this platform by the ICD
        // Loader."
        return returnString(platform_config::ICD_SUFFIX, param_value_size, param_value, param_value_size_ret);
    case CL_PLATFORM_NUMERIC_VERSION_KHR:
        // enabled by the cl_khr_extended_versioning extension
        // "Returns detailed (major, minor, patch) numeric version information. The major and minor version numbers
        // returned must match those returned via `CL_PLATFORM_VERSION`."
        return returnValue<cl_version_khr>(
            CL_MAKE_VERSION_KHR(1, 2, 0), param_value_size, param_value, param_value_size_ret);
    case CL_PLATFORM_EXTENSIONS_WITH_VERSION_KHR:
        // enabled by the cl_khr_extended_versioning extension
        // "Returns an array of description (name and version) structures. The same extension name must not be reported
        // more than once. The list of extensions reported must match the list reported via `CL_PLATFORM_EXTENSIONS`."
        return returnExtensions(platform_config::EXTENSIONS, param_value_size, param_value, param_value_size_ret);
#ifdef CL_VERSION_2_1 /* these are not really supported, but required to be present for OpenCL 3.0 support */
    case CL_PLATFORM_HOST_TIMER_RESOLUTION:
        //"This value must be 0 for devices that do not support Device and Host Timer Synchronization."
        return returnValue<cl_ulong>(0, param_value_size, param_value, param_value_size_ret);
#endif
    default:
        // invalid parameter-type
        return returnError(
            CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_platform_info value %d", param_name));
    }
}

Platform& Platform::getVC4CLPlatform()
{
    static std::unique_ptr<Platform> singleton(new Platform());
    return *singleton;
}

/*!
 * OpenCL 1.2 specification, page 33:
 *  The list of platforms available can be obtained using the following function.
 *
 *  \param num_entries is the number of cl_platform_id entries that can be added to platforms. If platforms is not NULL,
 * the num_entries must be greater than zero.
 *
 *  \param platforms returns a list of OpenCL platforms found.  The cl_platform_id values returned in platforms can be
 * used to identify a specific OpenCL platform. If platforms argument is NULL, this argument is ignored.  The number of
 * OpenCL platforms returned is the minimum of the value specified by num_entries or the number of OpenCL platforms
 * available.
 *
 *  \param num_platforms returns the number of OpenCL platforms available. If num_platforms is NULL, this argument is
 * ignored.
 *
 *  \return clGetPlatformIDs returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_VALUE if num_entries is equal to zero and platforms is not NULL or if both num_platforms and platforms
 * are NULL.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetPlatformIDs)(cl_uint num_entries, cl_platform_id* platforms, cl_uint* num_platforms)
{
    VC4CL_PRINT_API_CALL(
        "cl_int", clGetPlatformIDs, "cl_uint", num_entries, "cl_platform_id*", platforms, "cl_uint*", num_platforms);
    if(platforms == nullptr && num_platforms == nullptr)
        // can't return anything
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameters are empty!");

    if(num_entries == 0)
    {
        if(platforms != nullptr)
        {
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Cannot return 0 platforms!");
        }
    }

    if(platforms != nullptr)
        *platforms = Platform::getVC4CLPlatform().toBase();
    if(num_platforms != nullptr)
        // only one single platform
        *num_platforms = 1;
    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 33+:
 *  gets specific information about the OpenCL platform.  The information that can be queried using clGetPlatformInfo is
 * specified in table 4.1.
 *
 *  \param platform refers to the platform ID returned by clGetPlatformIDs or can be NULL.  If platform is NULL, the
 * behavior is implementation-defined.
 *
 *  \param param_name is an enumeration constant that identifies the platform information being queried. It can be one
 * of the following values as specified in table 4.1.
 *
 *  \param param_value is a pointer to memory location where appropriate values for a given param_name as specified in
 * table 4.1 will be returned. If param_value is NULL, it is ignored.
 *
 *  \param param_value_size specifies the size in bytes of memory pointed to by param_value.  This size in bytes must be
 * >= size of return type specified in table 4.1.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data being queried by param_value . If
 * param_value_size_ret is NULL, it is ignored.
 *
 *  \return clGetPlatformInfo returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_PLATFORM if platform is not a valid platform.
 *  - CL_INVALID_VALUE if param_name is not one of the supported values or if size in bytes specified by
 * param_value_size is < size of return type as specified in table 4.1 and param_value is not a NULL value.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetPlatformInfo)(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size,
    void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetPlatformInfo, "cl_platform_id", platform, "cl_platform_info", param_name,
        "size_t", param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_PLATFORM(platform)
    return toType<Platform>(platform)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
