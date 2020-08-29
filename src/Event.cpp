/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Event.h"
#include "queue_handler.h"

#include <chrono>

using namespace vc4cl;

EventAction::~EventAction() noexcept = default;
CustomAction::~CustomAction() noexcept = default;
NoAction::~NoAction() noexcept = default;

Event::Event(Context* context, cl_int status, CommandType type) :
    HasContext(context), type(type), queue(nullptr), status(status), userStatusSet(false)
{
    /*
     * Since we set the initial status for user events here to CL_SUBMITTED, we will never gather profiling information
     * for them.
     * This is okay, since according to OpenCL 1.2, section 5.12 user-events cannot be profiled anyway.
     */
}

Event::~Event() noexcept = default;

cl_int Event::setUserEventStatus(cl_int execution_status)
{
    if(execution_status != CL_COMPLETE && execution_status >= 0)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
            buildString("Event has already finished with status %d", execution_status));

    std::lock_guard<std::mutex> guard(statusLock);
    if(userStatusSet)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "User status has already been set!");

    status = execution_status;
    userStatusSet = true;

    return CL_SUCCESS;
}

cl_int Event::getInfo(
    cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) const
{
    switch(param_name)
    {
    case CL_EVENT_COMMAND_QUEUE:
        return returnValue<cl_command_queue>(!queue ? nullptr : const_cast<CommandQueue*>(queue.get())->toBase(),
            param_value_size, param_value, param_value_size_ret);
    case CL_EVENT_CONTEXT:
        return returnValue<cl_context>(
            const_cast<cl_context>(context()->toBase()), param_value_size, param_value, param_value_size_ret);
    case CL_EVENT_COMMAND_TYPE:
        return returnValue<cl_command_type>(
            static_cast<cl_command_type>(type), param_value_size, param_value, param_value_size_ret);
    case CL_EVENT_COMMAND_EXECUTION_STATUS:
    {
        std::lock_guard<std::mutex> guard(statusLock);
        return returnValue<cl_int>(status, param_value_size, param_value, param_value_size_ret);
    }
    case CL_EVENT_REFERENCE_COUNT:
        return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
    }

    return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_event_info value %d", param_name));
}

cl_int Event::setCallback(cl_int command_exec_callback_type, EventCallback callback, void* user_data)
{
    if(callback == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Cannot set a NULL callback!");
    if(command_exec_callback_type != CL_SUBMITTED && command_exec_callback_type != CL_RUNNING &&
        command_exec_callback_type != CL_COMPLETE)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
            buildString("Invalid type for event callback: %d!", command_exec_callback_type));

    std::lock_guard<std::mutex> guard(statusLock);
    if(status > command_exec_callback_type)
        callbacks.emplace_back(command_exec_callback_type, callback, user_data);
    else
        /*
         * The status at which the callback is be notified has already been triggered -> call immediately
         *
         * OpenCL 1.2, page 186: "All callbacks registered for an event object must be called"
         * And further:
         *   "If the callback is called as the result of the command associated with event being abnormally terminated,
         * an appropriate error code for the error that caused the termination will be passed to
         * event_command_exec_status instead."
         *
         * Also intel/beignet and pocl both do the same mechanism:
         * https://github.com/intel/beignet/blob/591d387327ce35f03a6152d4c823415729e221f2/src/cl_event.c#L346
         * https://github.com/pocl/pocl/blob/3f5a44a64ab7c7d5907c8cbf385ad7f13eff659a/lib/CL/clSetEventCallback.c#L39
         */
        // XXX intel/beignet sets the actual current status of the event, pocl the status for which the callback is
        // registered
        callback(toBase(), status < 0 /* error? */ ? status : command_exec_callback_type, user_data);

    return CL_SUCCESS;
}

cl_int Event::getProfilingInfo(
    cl_profiling_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) const
{
    if(!queue || !queue->isProfilingEnabled() || status != CL_COMPLETE || userStatusSet)
        return CL_PROFILING_INFO_NOT_AVAILABLE;

    switch(param_name)
    {
    case CL_PROFILING_COMMAND_QUEUED:
        return returnValue<cl_ulong>(profile.queue_time, param_value_size, param_value, param_value_size_ret);
    case CL_PROFILING_COMMAND_SUBMIT:
        return returnValue<cl_ulong>(profile.submit_time, param_value_size, param_value, param_value_size_ret);
    case CL_PROFILING_COMMAND_START:
        return returnValue<cl_ulong>(profile.start_time, param_value_size, param_value, param_value_size_ret);
    case CL_PROFILING_COMMAND_END:
        return returnValue<cl_ulong>(profile.end_time, param_value_size, param_value, param_value_size_ret);
    }

    return returnError(
        CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_profiling_info value %d", param_name));
}

cl_int Event::waitFor() const
{
    if(!checkReferences())
        return CL_INVALID_EVENT;
    if(type != CommandType::USER_COMMAND)
    {
        // not required by OpenCL standard, but guarantees we are not waiting on an event that is not scheduled. This
        // does not count for user-events which are never scheduled in a command queue.
        CHECK_COMMAND_QUEUE(queue.get())
    }

    for(auto& e : waitList)
    {
        if(e->waitFor() != CL_SUCCESS)
            return returnError(
                CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, __FILE__, __LINE__, "Error in event in wait-list");
    }

    if(!isFinished())
        // no need to lock the queue mutex if we are already done
        EventQueue::getInstance()->waitForEvent(this);
    return status;
}

bool Event::isFinished() const
{
    std::lock_guard<std::mutex> guard(statusLock);
    // an event is finished, if it has a state of CL_COMPLETE or any negative value (error-states)
    return status == CL_COMPLETE || status < 0;
}

cl_int Event::getStatus() const
{
    std::lock_guard<std::mutex> guard(statusLock);
    return status;
}

void Event::fireCallbacks(cl_int previousStatus)
{
    for(const auto& callback : callbacks)
    {
        if(previousStatus > std::get<0>(callback) && status <= std::get<0>(callback))
            //"The registered callback function will be called when the execution status of command associated with
            // event changes to an execution status equal to or past the status specified by command_exec_status"
            // we additionally check for previous status being "less advanced" than the expected one to make sure the
            // callback is only called once (when the status "passes by" the expected status).
            std::get<1>(callback)(toBase(), status, std::get<2>(callback));
    }
}

void Event::updateStatus(cl_int status, bool fireCallbacks)
{
    std::lock_guard<std::mutex> guard(statusLock);
    if(status == this->status)
        // don't repeat setting status, e.g. required in queue handler, if events on wait list are not done yet
        return;
    auto oldStatus = this->status;
    this->status = status;
    if(status == CL_SUBMITTED)
        setTime(profile.submit_time);
    else if(status == CL_RUNNING)
        setTime(profile.start_time);
    else
        setTime(profile.end_time);
    if(fireCallbacks)
        this->fireCallbacks(oldStatus);
}

CommandQueue* Event::getCommandQueue()
{
    return queue.get();
}

cl_int Event::prepareToQueue(CommandQueue* queue)
{
    if(this->queue || queue == nullptr)
        return CL_INVALID_COMMAND_QUEUE;
    CHECK_COMMAND_QUEUE(queue)
    this->queue.reset(queue);
    cl_int status = retain();
    if(status != CL_SUCCESS)
        return status;
    profile.end_time = 0;
    profile.queue_time = 0;
    profile.start_time = 0;
    profile.submit_time = 0;

    setTime(profile.queue_time);

    return CL_SUCCESS;
}

void Event::setEventWaitList(cl_uint numEvents, const cl_event* events)
{
    waitList.reserve(numEvents);
    for(cl_uint i = 0; i < numEvents; ++i)
    {
        Event* e = toType<Event>(events[i]);
        waitList.emplace_back(object_wrapper<Event>{e});
    }
}

cl_int Event::setAsResultOrRelease(cl_int condition, cl_event* event)
{
    if(condition == CL_SUCCESS && event != nullptr)
    {
        *event = toBase();
        return CL_SUCCESS;
    }
    // if the condition was an error, need to release the event too, but return the original error
    if(condition != CL_SUCCESS)
    {
        ignoreReturnValue(release(), __FILE__, __LINE__, "Already an error set to be returned");
        return returnError(condition, __FILE__, __LINE__, buildString("Aborting due to previous error: %d", condition));
    }
    // need to release once if the event is not used by the caller, since otherwise it cannot be completely freed
    return release();
}

WaitListStatus Event::getWaitListStatus() const
{
    // checks wait list, whether they all have finished and return if it was successfully
    for(const auto& event : waitList)
    {
        auto st = event->getStatus();
        if(st < 0)
            return WaitListStatus::ERROR;
        if(st != CL_COMPLETE)
            return WaitListStatus::PENDING;
    }
    return WaitListStatus::FINISHED;
}

void Event::clearWaitList()
{
    waitList.clear();
}

void Event::setTime(cl_ulong& field)
{
    const auto now = std::chrono::high_resolution_clock::now();
    field = static_cast<cl_ulong>(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}

/*!
 * OpenCL 1.2 specification, page 180:
 *
 *  Creates a user event object. User events allow applications to enqueue commands that wait on a user event to finish
 * before the command is executed by the device.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \retrun clCreateUserEvent returns a valid non-zero event object and errcode_ret is set to CL_SUCCESS if the user
 * event object is created successfully. Otherwise, it returns a NULL value with one of the following error values
 * returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  The execution status of the user event object created is set to CL_SUBMITTED .
 */
cl_event VC4CL_FUNC(clCreateUserEvent)(cl_context context, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_event", clCreateUserEvent, "cl_context", context, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_event)
    Event* event = newOpenCLObject<Event>(toType<Context>(context), CL_SUBMITTED, CommandType::USER_COMMAND);
    CHECK_ALLOCATION_ERROR_CODE(event, errcode_ret, cl_event)
    RETURN_OBJECT(event->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, pags 180+:
 *
 *  Sets the execution status of a user event object.
 *
 *  \param event is a user event object created using clCreateUserEvent.
 *
 *  \param execution_status specifies the new execution status to be set and can be CL_COMPLETE or a negative integer
 * value to indicate an error. A negative integer value causes all enqueued commands that wait on this user event to be
 * terminated. clSetUserEventStatus can only be called once to change the execution status of event.
 *
 *  \return clSetUserEventStatus returns CL_SUCCESS if the function was executed successfully. Otherwise, it returns one
 * of the following errors:
 *  - CL_INVALID_EVENT if event is not a valid user event object.
 *  - CL_INVALID_VALUE if the execution_status is not CL_COMPLETE or a negative integer value.
 *  - CL_INVALID_OPERATION if the execution_status for event has already been changed by a previous call to
 * clSetUserEventStatus.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clSetUserEventStatus)(cl_event event, cl_int execution_status)
{
    VC4CL_PRINT_API_CALL("cl_int", clSetUserEventStatus, "cl_event", event, "cl_int", execution_status);
    CHECK_EVENT(toType<Event>(event))
    return toType<Event>(event)->setUserEventStatus(execution_status);
}

/*!
 * OpenCL 1.2 specification, pages 181+:
 *
 *  Waits on the host thread for commands identified by event objects in event_list to complete. A command is considered
 * complete if its execution status is CL_COMPLETE or a negative value. The events specified in event_list act as
 * synchronization points.
 *
 *  \return clWaitForEvents returns CL_SUCCESS if the execution status of all events in event_list is CL_COMPLETE .
 * Otherwise, it returns one of the following errors:
 *  - CL_INVALID_VALUE if num_events is zero or event_list is NULL .
 *  - CL_INVALID_CONTEXT if events specified in event_list do not belong to the same context.
 *  - CL_INVALID_EVENT if event objects specified in event_list are not valid event objects.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the execution status of any of the events in event_list is a
 * negative integer value.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clWaitForEvents)(cl_uint num_events, const cl_event* event_list)
{
    VC4CL_PRINT_API_CALL("cl_int", clWaitForEvents, "cl_uint", num_events, "const cl_event*", event_list);
    if(num_events == 0 || event_list == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "No events to wait for!");

    CHECK_EVENT(toType<Event>(event_list[0]))
    Context* context = toType<Event>(event_list[0])->context();

    // first event is already checked above
    for(cl_uint i = 1; i < num_events; ++i)
    {
        CHECK_EVENT(toType<Event>(event_list[i]))
        if(toType<Event>(event_list[i])->context() != context)
            return returnError(CL_INVALID_CONTEXT, __FILE__, __LINE__, "Contexts of events do not match!");
    }

    bool with_errors = false;
    // wait for completion
    // in any case (error or not), we need to walk through all events to make sure they are all finished.
    for(cl_uint i = 0; i < num_events; ++i)
    {
        if(toType<Event>(event_list[i])->waitFor() != CL_COMPLETE)
            with_errors = true;
    }
    return with_errors ?
        returnError(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, __FILE__, __LINE__, "Error in event in wait-list!") :
        CL_SUCCESS;
}

// OpenCL 1.1 API function, deprecated in OpenCL 1.2
cl_int VC4CL_FUNC(clEnqueueWaitForEvents)(
    cl_command_queue command_queue, cl_uint num_events, const cl_event* event_list)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueWaitForEvents, "cl_command_queue", command_queue, "cl_uint", num_events,
        "const cl_event*", event_list);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))

    if(num_events == 0 || event_list == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "No events to wait for!");
    // since our command-queue is always in-order, do nothing
    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 182+:
 *
 *  Returns information about the event object.
 *
 *  \param event specifies the event object being queried.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information
 * returned in param_value by clGetEventInfo is described in table 5.18.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is
 * NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.18.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret
 * is NULL , it is ignored.
 *
 *  Using clGetEventInfo to determine if a command identified by event has finished execution (i.e.
 * CL_EVENT_COMMAND_EXECUTION_STATUS returns CL_COMPLETE ) is not a synchronization point. There are no guarantees that
 * the memory objects being modified by command associated with event will be visible to other enqueued commands.
 *
 *  \return clGetEventInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the
 * following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return
 * type as described in table 5.18 and param_value is not NULL .
 *  - CL_INVALID_VALUE if information to query given in param_name cannot be queried for event.
 *  - CL_INVALID_EVENT if event is a not a valid event object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetEventInfo)(
    cl_event event, cl_event_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetEventInfo, "cl_event", event, "cl_event_info", param_name, "size_t",
        param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_EVENT(toType<Event>(event))
    return toType<Event>(event)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

/*!
 * OpenCL 1.2 specification, pages 185+:
 *
 *  Registers a user callback function for a specific command execution status. The registered callback function will be
 * called when the execution status of command associated with event changes to an execution status equal to or past the
 * status specified by command_exec_status. Each call to clSetEventCallback registers the specified user callback
 * function on a callback stack associated with event. The order in which the registered user callback functions are
 * called is undefined.
 *
 *  \param event is a valid event object.
 *
 *  \param command_exec_callback_type specifies the command execution status for which the callback is registered. The
 * command execution callback values for which a callback can be registered are: CL_SUBMITTED , CL_RUNNING or
 * CL_COMPLETE . There is no guarantee that the callback functions registered for various execution status values for
 * an event will be called in the exact order that the execution status of a command changes. Furthermore, it should be
 * noted that receiving a call back for an event with a status other than CL_COMPLETE , in no way implies that the
 * memory model or execution model as defined by the OpenCL specification has changed. For example, it is not valid to
 * assume that a corresponding memory transfer has completed unless the event is in a state CL_COMPLETE .
 *
 *  \param pfn_event_notify is the event callback function that can be registered by the application. This callback
 * function may be called asynchronously by the OpenCL implementation. It is the application’s responsibility to ensure
 * that the callback function is thread-safe. The parameters to this callback function are:
 *   - event is the event object for which the callback function is invoked.
 *   - event_command_exec_status represents the execution status of command for which this callback function is invoked.
 * Refer to table 5.18 for the command execution status values. If the callback is called as the result of the command
 * associated with event being abnormally terminated, an appropriate error code for the error that caused the
 * termination will be passed to event_command_exec_status instead.
 *   - user_data is a pointer to user supplied data.
 *
 *  - user_data will be passed as the user_data argument when pfn_notify is called. user_data can be NULL .
 *
 *  All callbacks registered for an event object must be called. All enqueued callbacks shall be called before the event
 * object is destroyed. Callbacks must return promptly. The behavior of calling expensive system routines, OpenCL API
 * calls to create contexts or command-queues, or blocking OpenCL operations from the following list below, in a
 * callback is undefined.
 *
 *  If an application needs to wait for completion of a routine from the above list in a callback, please use the
 * non-blocking form of the function, and assign a completion callback to it to do the remainder of your work. Note that
 * when a callback (or other code) enqueues commands to a command-queue, the commands are not required to begin
 * execution until the queue is flushed. In standard usage, blocking enqueue calls serve this role by implicitly
 * flushing the queue. Since blocking calls are not permitted in callbacks, those callbacks that enqueue commands on a
 *  command queue should either call clFlush on the queue before returning or arrange for clFlush to be called later on
 * another thread.
 *
 *  \return clSetEventCallback returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_EVENT if event is not a valid event object .
 *  - CL_INVALID_VALUE if pfn_event_notify is NULL or if command_exec_callback_type is not CL_SUBMITTED , CL_RUNNING or
 * CL_COMPLETE .
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clSetEventCallback)(cl_event event, cl_int command_exec_callback_type,
    void(CL_CALLBACK* pfn_event_notify)(cl_event event, cl_int event_command_exec_status, void* user_data),
    void* user_data)
{
    VC4CL_PRINT_API_CALL("cl_int", clSetEventCallback, "cl_event", event, "cl_int", command_exec_callback_type,
        "void(CL_CALLBACK*)(cl_event event, cl_int event_command_exec_status, void* user_data)", &pfn_event_notify,
        "void*", user_data);
    CHECK_EVENT(toType<Event>(event))
    return toType<Event>(event)->setCallback(command_exec_callback_type, pfn_event_notify, user_data);
}

/*!
 * OpenCL 1.2 specification, pages 186+:
 *
 *  Increments the event reference count. The OpenCL commands that return an event perform an implicit retain.
 *
 *  \return clRetainEvent returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the
 * following errors:
 *  - CL_INVALID_EVENT if event is not a valid event object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clRetainEvent)(cl_event event)
{
    VC4CL_PRINT_API_CALL("cl_int", clRetainEvent, "cl_event", event);
    CHECK_EVENT(toType<Event>(event))
    return toType<Event>(event)->retain();
}

/*!
 * OpenCL 1.2 specification, page 187:
 *
 *  Decrements the event reference count.
 *
 *  \return clReleaseEvent returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the
 * following errors:
 *  - CL_INVALID_EVENT if event is not a valid event object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  The event object is deleted once the reference count becomes zero, the specific command identified by this event has
 * completed (or terminated) and there are no commands in the command-queues of a context that require a wait for this
 * event to complete.
 *
 *  NOTE: Developers should be careful when releasing their last reference count on events created by clCreateUserEvent
 * that have not yet been set to status of CL_COMPLETE or an error. If the user event was used in the event_wait_list
 * argument passed to a clEnqueue*** API or another application host thread is waiting for it in clWaitForEvents, those
 * commands and host threads will continue to wait for the event status to reach CL_COMPLETE or error, even after the
 * user has released the object. Since in this scenario the developer has released his last reference count to the user
 * event, it would be in principle no longer valid for him to change the status of the event to unblock all the other
 * machinery. As a result the waiting tasks will wait forever, and associated events, cl_mem objects, command queues
 * and contexts are likely to leak. In-order command queues caught up in this deadlock may cease to do any work.
 */
cl_int VC4CL_FUNC(clReleaseEvent)(cl_event event)
{
    VC4CL_PRINT_API_CALL("cl_int", clReleaseEvent, "cl_event", event);
    CHECK_EVENT(toType<Event>(event))
    return toType<Event>(event)->release();
}

/*!
 * OpenCL 1.2 specification, pages 192+:
 *
 *  Returns profiling information for the command associated with event.
 *
 *  \param event specifies the event object.
 *
 *  \param param_name specifies the profiling data to query. The list of supported param_name types and the information
 * returned in param_value by clGetEventProfilingInfo is described in table 5.19.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is
 * NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.19.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret
 * is NULL , it is ignored.
 *
 *  The unsigned 64-bit values returned can be used to measure the time in nano-seconds consumed by OpenCL commands.
 *
 *  OpenCL devices are required to correctly track time across changes in device frequency and power states. The
 * CL_DEVICE_PROFILING_TIMER_RESOLUTION specifies the resolution of the timer i.e. the number of nanoseconds elapsed
 * before the timer is incremented.
 *
 *  \return clGetEventProfilingInfo returns CL_SUCCESS if the function is executed successfully and the profiling
 * information has been recorded. Otherwise, it returns one of the following errors:
 *  - CL_PROFILING_INFO_NOT_AVAILABLE if the CL_QUEUE_PROFILING_ENABLE flag is not set for the command-queue, if the
 * execution status of the command identified by event is not CL_COMPLETE or if event is a user event object.
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return
 * type as described in table 5.19 and param_value is not NULL .
 *  - CL_INVALID_EVENT if event is a not a valid event object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetEventProfilingInfo)(cl_event event, cl_profiling_info param_name, size_t param_value_size,
    void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetEventProfilingInfo, "cl_event", event, "cl_profiling_info", param_name,
        "size_t", param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_EVENT(toType<Event>(event))
    return toType<Event>(event)->getProfilingInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
