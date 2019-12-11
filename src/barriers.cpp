/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#include "common.h"

#include "CommandQueue.h"
#include "Event.h"

using namespace vc4cl;

/*!
 * OpenCL 1.2 specification, pages 188+:
 *  Enqueues a marker command which waits for either a list of events to complete,
 * or if the list is empty it waits for all commands previously enqueued in command_queue to complete before it
 * completes. This command returns an event which can be waited on, i.e. this event can be waited on to insure that all
 * events either in the event_wait_list or all previously enqueued commands, queued before this command to
 * command_queue, have completed.
 *
 *  \param command_queue is a valid command-queue.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed.
 *
 *  If event_wait_list is NULL, num_events_in_wait_list must be 0. If event_wait_list is not NULL, the list of events
 * pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events specified
 * in event_wait_list act as synchronization points. The context associated with events in event_wait_list and
 * command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function
 * returns.
 *
 *  If event_wait_list is NULL, then this particular command waits until all previous enqueued commands to command_queue
 * have completed.
 *
 *  \param event returns an event object that identifies this particular command. Event objects are unique and can be
 * used to identify this marker command later on. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete. If the event_wait_list
 * and the event arguments are not NULL, the event argument should not refer to an element of the event_wait_list
 * array.
 *
 *  \return clEnqueueMarkerWithWaitList returns CL_SUCCESS if the function is successfully executed. Otherwise, it
 * returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueMarkerWithWaitList)(
    cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueMarkerWithWaitList, "cl_command_queue", command_queue, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))

    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    // since our commands are always executed in-order, no special handling is necessary (it always waits for all
    // events to finish)
    cl_int errcode = CL_SUCCESS;
    auto e = newOpenCLObject<Event>(toType<CommandQueue>(command_queue)->context(), CL_QUEUED, CommandType::MARKER);
    CHECK_ALLOCATION(e)
    EventAction* action = newObject<NoAction>(CL_SUCCESS);
    CHECK_ALLOCATION(action)
    e->action.reset(action);
    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    errcode = toType<CommandQueue>(command_queue)->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

// OpenCL 1.1 API function, deprecated in OpenCL 1.2
cl_int VC4CL_FUNC(clEnqueueMarker)(cl_command_queue command_queue, cl_event* event)
{
    return VC4CL_FUNC(clEnqueueMarkerWithWaitList)(command_queue, 0, nullptr, event);
}

/*!
 * OpenCL 1.2 specification, pages 189+
 *  Enqueues a barrier command which waits for either a list of events to complete, or if the list is empty it waits for
 * all commands previously enqueued in command_queue to complete before it completes.  This command blocks command
 * execution, that is, any following commands enqueued after it do not execute until it completes. This command returns
 * an event which can be waited on, i.e. this event can be waited on to insure that all events either in the
 * event_wait_list or all previously enqueued commands, queued before this command to command_queue, have completed.
 *
 *  \param command_queue is a valid command-queue.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not
 * NULL, the list of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater
 * than 0. The events specified in event_wait_list act as synchronization points. The context associated with events in
 * event_wait_list and command_queue must be the same. The memory associated with event_wait_list can be reused or
 * freed after the function returns.
 *
 *  If event_wait_list is NULL , then this particular command waits until all previous enqueued commands to
 * command_queue have completed.
 *
 *  \param event returns an event object that identifies this particular command.  Event objects are unique and can be
 * used to identify this barrier command later on. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete. If the event_wait_list
 * and the event arguments are not NULL, the event argument should not refer to an element of the event_wait_list
 * array.
 *
 *  \return clEnqueueBarrierWithWaitList returns CL_SUCCESS if the function is successfully executed. Otherwise, it
 * returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueBarrierWithWaitList)(
    cl_command_queue command_queue, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueBarrierWithWaitList, "cl_command_queue", command_queue, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))

    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    // since our commands are always executed in-order, no special handling is necessary (it always waits for all events
    // to finish)
    cl_int errcode = CL_SUCCESS;
    auto e = newOpenCLObject<Event>(toType<CommandQueue>(command_queue)->context(), CL_QUEUED, CommandType::BARRIER);
    CHECK_ALLOCATION(e)
    EventAction* action = newObject<NoAction>(CL_SUCCESS);
    CHECK_ALLOCATION(action)
    e->action.reset(action);
    e->setEventWaitList(num_events_in_wait_list, event_wait_list);
    errcode = toType<CommandQueue>(command_queue)->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

// OpenCL 1.1 API function, deprecated in OpenCL 1.2
cl_int VC4CL_FUNC(clEnqueueBarrier)(cl_command_queue command_queue)
{
    return VC4CL_FUNC(clEnqueueBarrierWithWaitList)(command_queue, 0, nullptr, nullptr);
}
