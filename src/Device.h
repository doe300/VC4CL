/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_DEVICE
#define VC4CL_DEVICE

#include "Object.h"

namespace vc4cl
{
    class Platform;

    class Device : public Object<_cl_device_id, CL_INVALID_DEVICE>
    {
    public:
        ~Device() override __attribute__((const));
        CHECK_RETURN cl_int getInfo(
            cl_device_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) const;

    private:
        Device();

        friend class Platform;
    };

} /* namespace vc4cl */

#endif /* VC4CL_DEVICE */
