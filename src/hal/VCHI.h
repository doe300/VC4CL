/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_VCHI
#define VC4VC4CL_VCHI

#include "hal.h"

struct opaque_vchi_instance_handle_t;
struct vchi_connection_t;

namespace vc4cl
{
    class VCHI
    {
    public:
        VCHI();
        ~VCHI();

        CHECK_RETURN ExecutionHandle executeQPU(unsigned numQPUs, std::pair<uint32_t*, uint32_t> controlAddress,
            bool flushBuffer, std::chrono::milliseconds timeout) const;

        bool readValue(SystemQuery query, uint32_t& output) noexcept;

    private:
        opaque_vchi_instance_handle_t* vchiHandle;
        vchi_connection_t* connectionHandle;
    };
} /* namespace vc4cl */

#endif /* VC4CL_VCHI */
