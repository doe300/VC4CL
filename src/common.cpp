/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "common.h"
#include "extensions.h"
#include "git_commit.h"

#include <CL/opencl.h>

#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <sys/prctl.h>

using namespace vc4cl;

// This is defined here to not need to include the git_commit.h header into the vc4cl_config.h header, since otherwise
// we would always do an almost complete fully rebuild (since the git_commit.h header is always updated)
const std::string platform_config::VERSION = std::string("OpenCL ") + platform_config::OPENCL_VERSION +
    std::string(" VC4CL ") + platform_config::VC4CL_VERSION + (" (" GIT_COMMIT ")");

std::string vc4cl::joinStrings(const std::vector<std::string>& strings, const std::string& delim)
{
    std::stringstream s;
    for(size_t i = 0; i < strings.size(); ++i)
    {
        if(i > 0)
            s << delim;
        s << strings.at(i);
    }
    return s.str();
}

cl_int vc4cl::returnValue(const void* value, const size_t value_size, const size_t value_count, size_t output_size,
    void* output, size_t* output_size_ret)
{
    if(output != nullptr && value != nullptr)
    {
        if(output_size < value_size * value_count)
            // not enough space on output parameter
            return CL_INVALID_VALUE;
        memcpy(output, value, value_size * value_count);
    }
    if(output_size_ret != nullptr)
        *output_size_ret = value_count * value_size;
    return CL_SUCCESS;
}

cl_int vc4cl::returnString(const std::string& string, size_t output_size, void* output, size_t* output_size_ret)
{
    const size_t string_length = string.length() + 1 /* 0-byte */;
    return returnValue(string.data(), sizeof(char), string_length, output_size, output, output_size_ret);
}

CHECK_RETURN cl_int vc4cl::returnBuffers(const std::vector<void*>& buffers, const std::vector<size_t>& sizes,
    size_t type_size, size_t output_size, void* output, size_t* output_size_ret)
{
    if(buffers.size() != sizes.size())
        return CL_INVALID_VALUE;
    // the parameter size is the size of the pointer array, not the size of the actual data
    auto inputSize = sizeof(void*) * sizes.size();
    if(output != nullptr)
    {
        if(output_size < inputSize)
            // not enough space on output parameter (for the pointer, the pointed-to data is not checked for size!)
            return CL_INVALID_VALUE;
        // copy the single buffers
        for(std::size_t i = 0; i < buffers.size(); ++i)
        {
            if(buffers.at(i) != nullptr && reinterpret_cast<void**>(output)[i] != nullptr)
                memcpy(reinterpret_cast<void**>(output)[i], buffers.at(i), sizes.at(i));
        }
    }
    if(output_size_ret != nullptr)
        *output_size_ret = inputSize;
    return CL_SUCCESS;
}

cl_int vc4cl::returnExtensions(
    const std::vector<Extension>& extensions, size_t output_size, void* output, size_t* output_size_ret)
{
    auto requiredSize = sizeof(cl_name_version_khr) * extensions.size();
    if(output != nullptr)
    {
        if(output_size < requiredSize)
            // not enough space on output parameter
            return CL_INVALID_VALUE;
        std::vector<cl_name_version_khr> items;
        items.reserve(extensions.size());
        for(const auto& ext : extensions)
        {
            cl_name_version_khr item{CL_MAKE_VERSION_KHR(ext.majorVersion, ext.minorVersion, 0), ""};
            strncpy(item.name, ext.name.data(), CL_NAME_VERSION_MAX_NAME_SIZE_KHR);
            items.emplace_back(std::move(item));
        }
        memcpy(output, items.data(), requiredSize);
    }

    if(output_size_ret != nullptr)
        *output_size_ret = requiredSize;
    return CL_SUCCESS;
}

static std::mutex logMutex;

std::unique_lock<std::mutex> vc4cl::lockLog()
{
    static const thread_local auto threadName = []() -> std::string {
        char buffer[32] = {0};
        prctl(PR_GET_NAME, buffer, 0, 0, 0);
        return buffer;
    }();
    std::unique_lock<std::mutex> lock(logMutex);
    std::cout << "[VC4CL](" << std::setfill(' ') << std::setw(15) << threadName << "): ";
    return lock;
}

static DebugLevel getDebugLevel()
{
    std::underlying_type<DebugLevel>::type level = 0;
    if(auto env = std::getenv("VC4CL_DEBUG"))
    {
        std::string tmp(env);
        if(tmp.find("api") != std::string::npos)
            level |= static_cast<uint8_t>(DebugLevel::API_CALLS);
        if(tmp.find("code") != std::string::npos)
            level |= static_cast<uint8_t>(DebugLevel::DUMP_CODE);
        if(tmp.find("syscall") != std::string::npos)
            level |= static_cast<uint8_t>(DebugLevel::SYSCALL);
        if(tmp.find("execution") != std::string::npos)
            level |= static_cast<uint8_t>(DebugLevel::KERNEL_EXECUTION);
        if(tmp.find("events") != std::string::npos)
            level |= static_cast<uint8_t>(DebugLevel::EVENTS);
        if(tmp.find("objects") != std::string::npos)
            level |= static_cast<uint8_t>(DebugLevel::OBJECTS);
        if(tmp.find("all") != std::string::npos)
            level = static_cast<uint8_t>(DebugLevel::ALL);
    }

    return static_cast<DebugLevel>(level);
}

bool vc4cl::isDebugModeEnabled(DebugLevel level)
{
    static const auto debugLevel = getDebugLevel();
    return (static_cast<uint8_t>(debugLevel) & static_cast<uint8_t>(level)) == static_cast<uint8_t>(level);
}
