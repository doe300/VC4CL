/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef VC4CL_CONTEXT_H
#define VC4CL_CONTEXT_H

#include "Device.h"
#include "Platform.h"

namespace vc4cl
{
    using ContextErrorCallback = void(CL_CALLBACK*)(
        const char* errinfo, const void* private_info, size_t cb, void* user_data);
    using ContextReleaseCallback = void(CL_CALLBACK*)(cl_context context, void* user_data);

    class Context final : public Object<_cl_context, CL_INVALID_CONTEXT>
    {
    public:
        Context(const Device* device, const Platform* platform, const std::vector<cl_context_properties>& explicitProperties,
            ContextErrorCallback callback = nullptr, void* userData = nullptr);
        ~Context() noexcept override;
        CHECK_RETURN cl_int getInfo(
            cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

        void fireCallback(const std::string& errorInfo, const void* privateInfo, size_t cb);
        bool initializeMemoryToZero(cl_context_properties memoryType) const __attribute__((pure));

        CHECK_RETURN cl_int setReleaseCallback(ContextReleaseCallback callback, void* userData);

        const Device* device;

    private:
        // properties
        const Platform* platform;
        const std::vector<cl_context_properties> explicitProperties;

        // callbacks
        const ContextErrorCallback callback;
        void* userData;
        std::vector<std::pair<ContextReleaseCallback, void*>> callbacks;
    };

    class HasContext
    {
    public:
        explicit HasContext(Context* context) : c(context) {}

        const Context* context() const __attribute__((pure));
        Context* context() __attribute__((pure));

    private:
        object_wrapper<Context> c;
    };

} /* namespace vc4cl */

#endif /* VC4CL_CONTEXT_H */
