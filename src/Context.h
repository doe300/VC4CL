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
    using ContextCallback = void(CL_CALLBACK*)(
        const char* errinfo, const void* private_info, size_t cb, void* user_data);

    enum class ContextProperty
    {
        NONE = 0,
        USER_SYNCHRONISATION = 1,
        PLATFORM = 2,
        // provided by cl_khr_initialize_memory
        INITIALIZE_MEMORY = 4,
    };

    constexpr ContextProperty operator|(ContextProperty one, ContextProperty other) noexcept
    {
        return static_cast<ContextProperty>(static_cast<unsigned>(one) | static_cast<unsigned>(other));
    }

    constexpr bool operator&(ContextProperty one, ContextProperty other) noexcept
    {
        return (static_cast<unsigned>(one) & static_cast<unsigned>(other)) != 0;
    }

    class Context final : public Object<_cl_context, CL_INVALID_CONTEXT>
    {
    public:
        Context(const Device* device, bool userSync, cl_context_properties memoryToZeroOut, const Platform* platform,
            ContextProperty explicitProperties, ContextCallback callback = nullptr, void* userData = nullptr);
        ~Context() noexcept override;
        CHECK_RETURN cl_int getInfo(
            cl_context_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

        void fireCallback(const std::string& errorInfo, const void* privateInfo, size_t cb);
        bool initializeMemoryToZero(cl_context_properties memoryType) const __attribute__((pure));

        const Device* device;

    private:
        // properties
        const bool userSync;
        const Platform* platform;
        const ContextProperty explicitProperties;
        const cl_context_properties memoryToInitialize;

        // callback
        const ContextCallback callback;
        void* userData;
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
