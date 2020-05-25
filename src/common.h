/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include "types.h"

#include <array>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#if __cplusplus > 201402L
#define CHECK_RETURN [[nodiscard]]
#else
#define CHECK_RETURN __attribute__((warn_unused_result))
#endif

namespace vc4cl
{
    std::unique_lock<std::mutex> lockLog();

    /**
     * The different modes/levels of debug output.
     *
     * This is a bitset allowing the fields to be combined.
     */
    enum class DebugLevel : uint8_t
    {
        NONE = 0,
        // OpenCL API calls, parameters and return values (on error) are logged to the standard output
        API_CALLS = 1u << 0u,
        // OpenCL C, IR sources and VC4C binary output are dumped to temporary files
        DUMP_CODE = 1u << 1u,
        // Detailed information before and after syscalls (e.g. mailbox) is logged to the standard output
        SYSCALL = 1u << 2u,
        // Kernel execution, parameter and work-group information is logged to the standard output
        KERNEL_EXECUTION = 1u << 3u,
        // Event information is logged to the standard output
        EVENTS = 1u << 4u,
        // Lifetime information of OpenCL objects is logged to the standard output
        OBJECTS = 1 << 5u,
        ALL = 0xFF

    };

#define DEBUG_LOG(debugLevel, param)                                                                                   \
    if(isDebugModeEnabled(debugLevel))                                                                                 \
    {                                                                                                                  \
        auto lock = lockLog();                                                                                         \
        param;                                                                                                         \
    }

    bool isDebugModeEnabled(DebugLevel level);

    CHECK_RETURN std::string joinStrings(const std::vector<std::string>& strings, const std::string& delim = " ");

    template <typename T, typename Func = std::function<std::string(const T&)>>
    CHECK_RETURN std::string joinStrings(const std::vector<T>& objects, Func&& f, const std::string& delim = " ")
    {
        std::vector<std::string> strings;
        strings.reserve(objects.size());
        for(const auto& obj : objects)
            strings.emplace_back(f(obj));
        return joinStrings(strings, delim);
    }

    CHECK_RETURN cl_int returnValue(const void* value, size_t value_size, size_t value_count, size_t output_size,
        void* output, size_t* output_size_ret);
    CHECK_RETURN cl_int returnString(
        const std::string& string, size_t output_size, void* output, size_t* output_size_ret);
    CHECK_RETURN cl_int returnBuffers(const std::vector<void*>& buffers, const std::vector<size_t>& sizes,
        size_t type_size, size_t output_size, void* output, size_t* output_size_ret);
    CHECK_RETURN cl_int returnExtensions(
        const std::vector<Extension>& extensions, size_t output_size, void* output, size_t* output_size_ret);

    template <typename T>
    CHECK_RETURN typename std::enable_if<std::is_arithmetic<T>::value | std::is_pointer<T>::value, cl_int>::type
    returnValue(const T value, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
    {
        const T tmp = value;
        return returnValue(&tmp, sizeof(T), 1, param_value_size, param_value, param_value_size_ret);
    }

    template <typename T>
    CHECK_RETURN inline T returnError(
        cl_int error, cl_int* errcode_ret, const char* file, unsigned line, const std::string& reason)
    {
        DEBUG_LOG(DebugLevel::API_CALLS,
            std::cout << "Error in '" << file << ":" << line << "', returning status " << error << ": " << reason
                      << std::endl)
        if(errcode_ret != nullptr)
            *errcode_ret = error;
        return nullptr;
    }

    CHECK_RETURN inline cl_int returnError(cl_int error, const char* file, unsigned line, const std::string& reason)
    {
        DEBUG_LOG(DebugLevel::API_CALLS,
            std::cout << "Error in '" << file << ":" << line << "', returning status " << error << ": " << reason
                      << std::endl)
        return error;
    }

    inline void ignoreReturnValue(cl_int state, const char* file, unsigned line, const char* reasonForIgnoring)
    {
        // used to hide warnings of unused return-values
        // the reason is for documentation only
        if(state != CL_SUCCESS)
            DEBUG_LOG(DebugLevel::API_CALLS,
                std::cout << "Error in '" << file << ":" << line << "', returning status " << state << std::endl);
    }

    template <typename... T>
    CHECK_RETURN inline std::string buildString(const char* format, T... args)
    {
        std::array<char, 4096> tmp = {0};
        int num = snprintf(tmp.data(), tmp.size(), format, std::forward<T>(args)...);
        return std::string(tmp.data(), static_cast<unsigned>(num));
    }

    template <typename T, typename Src>
    CHECK_RETURN inline T* toType(Src* ptr) noexcept
    {
        if(ptr == nullptr)
            return nullptr;
        return reinterpret_cast<T*>(ptr->object);
    }

    template <typename T, typename... Args>
    CHECK_RETURN inline T* newObject(Args&&... args)
    {
        try
        {
            return new T(std::forward<Args>(args)...);
        }
        catch(std::bad_alloc&)
        {
            // so we can return CL_OUT_OF_HOST_MEMORY
            return nullptr;
        }
    }

        //
        // TYPE HELPER MACROS
        //
#define CHECK_OBJECT(obj, error)                                                                                       \
    if((obj) == nullptr || !(obj)->checkReferences())                                                                  \
        return returnError(error, __FILE__, __LINE__, "Object validity check failed!");
#define CHECK_OBJECT_ERROR_CODE(obj, error, errcode_ret, type)                                                         \
    if((obj) == nullptr || !(obj)->checkReferences())                                                                  \
        return returnError<type>((error), (errcode_ret), __FILE__, __LINE__, "Object validity check failed!");

#define CHECK_ALLOCATION(obj)                                                                                          \
    if((obj) == nullptr)                                                                                               \
        return returnError(CL_OUT_OF_HOST_MEMORY, __FILE__, __LINE__, "Allocation failed!");
#define CHECK_ALLOCATION_ERROR_CODE(obj, errcode_ret, type)                                                            \
    if((obj) == nullptr)                                                                                               \
        return returnError<type>(CL_OUT_OF_HOST_MEMORY, (errcode_ret), __FILE__, __LINE__, "Allocation failed!");

#define RETURN_OBJECT(object, errcode_ret)                                                                             \
    if((errcode_ret) != nullptr)                                                                                       \
        *(errcode_ret) = CL_SUCCESS;                                                                                   \
    return object;

#define CHECK_PLATFORM(platform)                                                                                       \
    if((platform) != nullptr && (platform) != Platform::getVC4CLPlatform().toBase())                                   \
        return returnError(CL_INVALID_PLATFORM, __FILE__, __LINE__, "Platform is not the VC4CL platform!");
#define CHECK_PLATFORM_ERROR_CODE(platform, errcode_ret, type)                                                         \
    if((platform) != nullptr && (platform) != VC4CL_PLATFORM.get())                                                    \
        return returnError<type>(CL_INVALID_PLATFORM, errcode_ret);

#define CHECK_DEVICE(device) CHECK_OBJECT(device, CL_INVALID_DEVICE)
#define CHECK_DEVICE_ERROR_CODE(device, errcode_ret, type)                                                             \
    CHECK_OBJECT_ERROR_CODE(device, CL_INVALID_DEVICE, errcode_ret, type)

#define CHECK_CONTEXT(context) CHECK_OBJECT(context, CL_INVALID_CONTEXT)
#define CHECK_CONTEXT_ERROR_CODE(context, errcode_ret, type)                                                           \
    CHECK_OBJECT_ERROR_CODE(context, CL_INVALID_CONTEXT, errcode_ret, type)

#define CHECK_DEVICE_WITH_CONTEXT(dev, context)                                                                        \
    CHECK_DEVICE(dev)                                                                                                  \
    if((context)->device != (dev))                                                                                     \
        return returnError(CL_INVALID_DEVICE, __FILE__, __LINE__, "Device does not match the Context's device!");

#define CHECK_COMMAND_QUEUE(queue) CHECK_OBJECT(queue, CL_INVALID_COMMAND_QUEUE)
#define CHECK_COMMAND_QUEUE_ERROR_CODE(queue, errcode_ret, type)                                                       \
    CHECK_OBJECT_ERROR_CODE(queue, CL_INVALID_COMMAND_QUEUE, errcode_ret, type)

#define CHECK_BUFFER(buffer) CHECK_OBJECT(buffer, CL_INVALID_MEM_OBJECT)
#define CHECK_BUFFER_ERROR_CODE(buffer, errcode_ret, type)                                                             \
    CHECK_OBJECT_ERROR_CODE(buffer, CL_INVALID_MEM_OBJECT, errcode_ret, type)

#define CHECK_SAMPLER(sampler) CHECK_OBJECT(sampler, CL_INVALID_SAMPLER)
#define CHECK_SAMPLER_ERROR_CODE(sampler, errcode_ret, type)                                                           \
    CHECK_OBJECT_ERROR_CODE(sampler, CL_INVALID_SAMPLER, errcode_ret, type)

#define CHECK_PROGRAM(program) CHECK_OBJECT(program, CL_INVALID_PROGRAM)
#define CHECK_PROGRAM_ERROR_CODE(program, errcode_ret, type)                                                           \
    CHECK_OBJECT_ERROR_CODE(program, CL_INVALID_PROGRAM, errcode_ret, type)

#define CHECK_KERNEL(kernel) CHECK_OBJECT(kernel, CL_INVALID_KERNEL)
#define CHECK_KERNEL_ERROR_CODE(kernel, errcode_ret, type)                                                             \
    CHECK_OBJECT_ERROR_CODE(kernel, CL_INVALID_KERNEL, errcode_ret, type)

#define CHECK_EVENT(event) CHECK_OBJECT(event, CL_INVALID_EVENT)
#define CHECK_EVENT_ERROR_CODE(event, errcode_ret, type)                                                               \
    CHECK_OBJECT_ERROR_CODE(event, CL_INVALID_EVENT, errcode_ret, type)

#define CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)                                                \
    if(((event_wait_list) == nullptr) != ((num_events_in_wait_list) == 0))                                             \
        return returnError(                                                                                            \
            CL_INVALID_EVENT_WAIT_LIST, __FILE__, __LINE__, "Event list does not match the number of elements!");
#define CHECK_EVENT_WAIT_LIST_ERROR_CODE(event_wait_list, num_events_in_wait_list, errcode_ret, type)                  \
    if(((event_wait_list) == nullptr) != ((num_events_in_wait_list) == 0))                                             \
        return returnError<type>(                                                                                      \
            CL_INVALID_EVENT_WAIT_LIST, errcode_ret, __FILE__, __LINE__, "Event list validity check failed!");

#define CHECK_COUNTER(counter) CHECK_OBJECT(counter, CL_INVALID_PERFORMANCE_COUNTER_VC4CL)
#define CHECK_COUNTER_ERROR_CODE(counter, errcode_ret, type)                                                           \
    CHECK_OBJECT_ERROR_CODE(context, CL_INVALID_PERFORMANCE_COUNTER_VC4CL, errcode_ret, type)

    //
    // OTHER HELPERS
    //
    template <typename T>
    constexpr bool exceedsLimits(const T val, const T min, const T max) noexcept
    {
        return val < min || val > max;
    }

    template <typename T>
    constexpr bool hasFlag(const T val, const T flag) noexcept
    {
        return val & flag;
    }

    constexpr bool moreThanOneMemoryAccessFlagSet(cl_mem_flags flags) noexcept
    {
        return (static_cast<unsigned>(hasFlag<cl_mem_flags>(flags, CL_MEM_WRITE_ONLY)) +
                   static_cast<unsigned>(hasFlag<cl_mem_flags>(flags, CL_MEM_READ_ONLY)) +
                   static_cast<unsigned>(hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE))) > 1;
    }

    constexpr bool moreThanOneHostAccessFlagSet(cl_mem_flags flags) noexcept
    {
        return (static_cast<unsigned>(hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_WRITE_ONLY)) +
                   static_cast<unsigned>(hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_READ_ONLY)) +
                   static_cast<unsigned>(hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_NO_ACCESS))) > 1;
    }

    template <typename T>
    std::ostream& printAPICallInternal(std::ostream& s, const char* const paramName, T param)
    {
        return s << paramName << " " << param;
    }

    template <>
    inline std::ostream& printAPICallInternal<const char*>(
        std::ostream& s, const char* const paramName, const char* param)
    {
        if(param == nullptr)
            return s << paramName << " (null)";
        else if(strlen(param) == 0)
            return s << paramName << " " << reinterpret_cast<const void*>(param);
        else
            return s << paramName << " \"" << param << '"';
    }

    template <typename T, typename... U>
    std::ostream& printAPICallInternal(std::ostream& s, const char* paramName, T param, U... args)
    {
        printAPICallInternal(s, paramName, param) << ", ";
        return printAPICallInternal(s, args...);
    }

    template <typename... T>
    inline void printAPICall(const char* const retType, const char* const funcName, T... args)
    {
        DEBUG_LOG(DebugLevel::API_CALLS, std::cout << "API call: " << retType << " " << funcName << "(";
                  printAPICallInternal(std::cout, args...) << ")" << std::endl)
    }

#define VC4CL_PRINT_API_CALL(retType, funcName, ...) vc4cl::printAPICall(retType, #funcName, __VA_ARGS__)
} // namespace vc4cl
#endif /* CONFIG_H */
