/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_PLATFORM
#define VC4CL_PLATFORM

#include "Device.h"

namespace vc4cl
{
    class Platform : public Object<_cl_platform_id, CL_INVALID_PLATFORM>
    {
    public:
        Platform(const Platform&) = delete;
        Platform(Platform&&) = delete;
        ~Platform() override __attribute__((const));

        Platform& operator=(const Platform&) = delete;
        Platform& operator=(Platform&&) = delete;

        CHECK_RETURN cl_int getInfo(cl_platform_info param_name, size_t param_value_size, void* param_value,
            size_t* param_value_size_ret) const;

        static Platform& getVC4CLPlatform();

        Device VideoCoreIVGPU;

    private:
        Platform();
    };

} /* namespace vc4cl */

#endif /* VC4CL_PLATFORM */
