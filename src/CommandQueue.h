/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#ifndef VC4CL_COMMANDQUEUE_H
#define VC4CL_COMMANDQUEUE_H

#include "Context.h"

namespace vc4cl
{
    class Event;
    class EventQueue;

    class CommandQueue final : public Object<_cl_command_queue, CL_INVALID_COMMAND_QUEUE>, public HasContext
    {
    public:
        CommandQueue(Context* context, bool outOfOrderExecution, bool profiling);
        ~CommandQueue() noexcept override;

        CHECK_RETURN cl_int getInfo(cl_command_queue_info param_name, size_t param_value_size, void* param_value,
            size_t* param_value_size_ret) const;

        CHECK_RETURN cl_int waitForWaitListFinish(const cl_event* waitList, cl_uint numEvents) const;
        CHECK_RETURN cl_int enqueueEvent(Event* event);
        cl_int setProperties(cl_command_queue_properties properties, bool enable);

        cl_int flush() __attribute__((const));
        cl_int finish();

        bool isProfilingEnabled() const __attribute__((pure));

    private:
        // properties
        bool outOfOrderExecution;
        bool profiling;
        std::shared_ptr<EventQueue> queue;
    };

} /* namespace vc4cl */

#endif /* VC4CL_COMMANDQUEUE_H */
