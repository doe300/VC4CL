/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */
#include "CommandQueue.h"

#include "Buffer.h"
#include "Event.h"
#include "Kernel.h"
#include "queue_handler.h"

using namespace vc4cl;

CommandQueue::CommandQueue(Context* context, const bool outOfOrderExecution, const bool profiling) :
    HasContext(context), outOfOrderExecution(outOfOrderExecution), profiling(profiling),
    queue(EventQueue::getInstance())
{
}

CommandQueue::~CommandQueue() noexcept = default;

cl_int CommandQueue::getInfo(
    cl_command_queue_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) const
{
    cl_command_queue_properties properties = (outOfOrderExecution ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0) |
        (profiling ? CL_QUEUE_PROFILING_ENABLE : 0);

    switch(param_name)
    {
    case CL_QUEUE_CONTEXT:
        //"Return the context specified when the command-queue is created."
        return returnValue<cl_context>(
            const_cast<cl_context>(context()->toBase()), param_value_size, param_value, param_value_size_ret);
    case CL_QUEUE_DEVICE:
        //"Return the device specified when the command-queue is created."
        return returnValue<cl_device_id>(
            const_cast<cl_device_id>(context()->device->toBase()), param_value_size, param_value, param_value_size_ret);
    case CL_QUEUE_REFERENCE_COUNT:
        // Return the command-queue reference count."
        return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
    case CL_QUEUE_PROPERTIES:
        //"Return the currently specified properties for the command-queue. These properties are specified by the
        // properties argument in clCreateCommandQueue."
        return returnValue<cl_command_queue_properties>(
            properties, param_value_size, param_value, param_value_size_ret);
#ifdef CL_VERSION_2_1
    case CL_QUEUE_DEVICE_DEFAULT:
        // "Returns 0 or NULL if the device associated with command_queue does not support On-Device Queues."
        return returnValue<cl_command_queue>(nullptr, param_value_size, param_value, param_value_size_ret);
#endif
#ifdef CL_VERSION_3_0
    case CL_QUEUE_PROPERTIES_ARRAY:
        // "Return the properties argument specified in clCreateCommandQueueWithProperties."
        // "If command_queue was created using clCreateCommandQueue, or if the properties argument specified in
        // clCreateCommandQueueWithProperties was NULL, the implementation may return either a param_value_size_ret of
        // 0 (i.e. there is are no properties to be returned), or the implementation may return a property value of 0
        // (where 0 is used to terminate the properties list)."
        // TODO to be precise, must return NULL or 0 property if not created with clCreateCommandQueueWithProperties()
        return returnValue<cl_command_queue_properties>(0, param_value_size, param_value, param_value_size_ret);
#endif
    }

    return returnError(
        CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_command_queue_info value %u", param_name));
}

cl_int CommandQueue::waitForWaitListFinish(const cl_event* waitList, cl_uint numEvents) const
{
    CHECK_EVENT_WAIT_LIST(waitList, numEvents)

    bool with_errors = false;
    // wait for completion
    for(cl_uint i = 0; i < numEvents; ++i)
    {
        if(toType<Event>(waitList[i])->waitFor() != CL_COMPLETE)
            with_errors = true;
    }
    return with_errors ?
        returnError(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, __FILE__, __LINE__, "Error in event in wait-list!") :
        CL_SUCCESS;
}

cl_int CommandQueue::enqueueEvent(Event* event)
{
    if(!checkReferences())
        return CL_INVALID_COMMAND_QUEUE;
    CHECK_EVENT(event)

    if(event->isFinished())
        // cannot execute already finished/failed event
        return returnError(CL_INVALID_EVENT, __FILE__, __LINE__,
            buildString("Event has already finished with status: %d", event->getStatus()));
    if(!event->action)
        return CL_INVALID_EVENT;

    cl_int status = event->prepareToQueue(this);

    // add to queue
    if(status == CL_SUCCESS)
        queue->pushEvent(event);

    return status;
}

cl_int CommandQueue::setProperties(cl_command_queue_properties properties, bool enable)
{
    if((properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) == CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)
        outOfOrderExecution = enable;
    if((properties & CL_QUEUE_PROFILING_ENABLE) == CL_QUEUE_PROFILING_ENABLE)
        profiling = enable;
    return CL_SUCCESS;
}

cl_int CommandQueue::flush()
{
    // doesn't do anything, since commands/events are automatically queued
    return CL_SUCCESS;
}

cl_int CommandQueue::finish()
{
    //"[...] blocks until all previously queued OpenCL commands in command_queue are issued to the associated device and
    // have completed"

    // wait_for_event_finish for all events in THIS queue
    while(auto event = queue->peek(this))
        ignoreReturnValue(event->waitFor(), __FILE__, __LINE__,
            "This method does not check the states of the single events as per specification");

    return CL_SUCCESS;
}

bool CommandQueue::isProfilingEnabled() const
{
    return profiling;
}

/*!
 * OpenCL 1.2 specification, pages 62+:
 *  Creates a command-queue on a specific device.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param device must be a device associated with context.  It can either be in the list of devices specified when
 * context is created using clCreateContext or have the same device type as device type specified when context is
 * created using clCreateContextFromType.
 *
 *  \param properties specifies a list of properties for the command-queue.  This is a bit-field and is described in
 * table 5.1. Only command-queue properties specified in table 5.1 can be set in properties; otherwise the value
 * specified in properties is considered to be not valid.
 *
 *  \param errcode_ret will return an appropriate error code.  If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateCommandQueue returns a valid non-zero command-queue and errcode_ret is set to CL_SUCCESS if the
 * command-queue is created successfully. Otherwise, it returns a NULL value with one of the following error values
 * returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_DEVICE if device is not a valid device or is not associated with context.
 *  - CL_INVALID_VALUE if values specified in properties are not valid.
 *  - CL_INVALID_QUEUE_PROPERTIES if values specified in properties are valid but are not supported by the device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_command_queue VC4CL_FUNC(clCreateCommandQueue)(
    cl_context context, cl_device_id device, cl_command_queue_properties properties, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_command_queue", clCreateCommandQueue, "cl_context", context, "cl_device_id", device,
        "cl_command_queue_properties", properties, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_command_queue)
    if(toType<Context>(context)->device != toType<Device>(device))
        return returnError<cl_command_queue>(
            CL_INVALID_DEVICE, errcode_ret, __FILE__, __LINE__, "Device of context does not match given device!");

    //"Determines whether the commands queued in the command-queue are executed in-order or out-of-order.
    // If set, the commands in the command-queue are executed out-of-order.
    // Otherwise, commands are executed in-order."
    bool out_of_order_execution =
        (properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE) == CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE;
    //"Enable or disable profiling of commands in the command-queue"
    bool profiling = (properties & CL_QUEUE_PROFILING_ENABLE) == CL_QUEUE_PROFILING_ENABLE;

    CommandQueue* queue = newOpenCLObject<CommandQueue>(toType<Context>(context), out_of_order_execution, profiling);
    CHECK_ALLOCATION_ERROR_CODE(queue, errcode_ret, cl_command_queue)
    RETURN_OBJECT(queue->toBase(), errcode_ret)
}

/*!
 * OpenCL 2.2 specification, pages 81+:
 *
 *  Creates a host or device command-queue on a specific device.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param device must be a device or sub-device associated with context.  It can either be in the list of devices and
 * sub-devices specified when context is created using clCreateContext or be a root device with the same device type as
 * specified when context is created using clCreateContextFromType.
 *
 *  \param properties specifies a list of properties for the command-queue and their corresponding values. Each property
 * name is immediately followed by the corresponding desired value. The list is terminated with 0. The list of supported
 * properties is described in the table below. If a supported property and its value is not specified in properties,
 * its default value will be used. properties can be NULL in which case the default values for supported command-queue
 * properties will be used.
 *
 *  \param errcode_ret will return an appropriate error code.  If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateCommandQueueWithProperties returns a valid non-zero command-queue and errcode_ret is set to
 * CL_SUCCESS if the command-queue is created successfully. Otherwise, it returns a NULL value with one of the following
 * error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_DEVICE if device is not a valid device or is not associated with context.
 *  - CL_INVALID_VALUE if values specified in properties are not valid.
 *  - CL_INVALID_QUEUE_PROPERTIES if values specified in properties are valid but are not supported by the device.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
#ifdef CL_VERSION_2_0
cl_command_queue VC4CL_FUNC(clCreateCommandQueueWithProperties)(
    cl_context context, cl_device_id device, const cl_queue_properties* properties, cl_int* errcode_ret)
{
    return VC4CL_FUNC(clCreateCommandQueueWithPropertiesKHR)(context, device, properties, errcode_ret);
}
#endif
cl_command_queue VC4CL_FUNC(clCreateCommandQueueWithPropertiesKHR)(
    cl_context context, cl_device_id device, const cl_queue_properties_khr* properties, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_command_queue", clCreateCommandQueueWithProperties, "cl_context", context, "cl_device_id",
        device, "const cl_queue_properties*", properties, "cl_int*", errcode_ret);
    cl_command_queue_properties props = 0;
    if(properties != nullptr)
    {
        const cl_queue_properties_khr* prop = properties;
        while(*prop != 0)
        {
            if(*prop == CL_QUEUE_PROPERTIES)
            {
                ++prop;
                props = *prop;
                ++prop;
            }
            else
                // any other property is not supported
                return returnError<cl_command_queue>(CL_INVALID_QUEUE_PROPERTIES, errcode_ret, __FILE__, __LINE__,
                    "Unsupported command-queue properties");
        }
    }

    return VC4CL_FUNC(clCreateCommandQueue)(context, device, props, errcode_ret);
}

/*!
 * OpenCL 1.2 specification, pages 63+:
 *  Increments the command_queue reference count.
 *
 *  \return clRetainCommandQueue returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns one
 * of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  clCreateCommandQueue performs an implicit retain.  This is very helpful for 3rd party libraries, which typically get
 * a command-queue passed to them by the application. However, it is possible that the application may delete the
 * command-queue without informing the library. Allowing functions to attach to (i.e. retain) and release a
 *  command-queue solves the problem of a command-queue being used by a library no longer being valid.
 */
cl_int VC4CL_FUNC(clRetainCommandQueue)(cl_command_queue command_queue)
{
    VC4CL_PRINT_API_CALL("cl_int", clRetainCommandQueue, "cl_command_queue", command_queue);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    return toType<CommandQueue>(command_queue)->retain();
}

/*!
 * OpenCL 1.2 specification, page 64:
 *  Decrements the command_queue reference count.
 *
 *  \return clReleaseCommandQueue returns CL_SUCCESS if the function is executed successfully.  Otherwise, it returns
 * one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  After the command_queue reference count becomes zero and all commands queued to command_queue have finished (eg.
 * kernel executions, memory object updates etc.), the command-queue is deleted.
 *
 *  clReleaseCommandQueue performs an implicit flush to issue any previously queued OpenCL commands in command_queue.
 */
cl_int VC4CL_FUNC(clReleaseCommandQueue)(cl_command_queue command_queue)
{
    VC4CL_PRINT_API_CALL("cl_int", clReleaseCommandQueue, "cl_command_queue", command_queue);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    return toType<CommandQueue>(command_queue)->release();
}

/*!
 * OpenCL 1.2 specification, pages 64+:
 *  Query information about a command-queue.
 *
 *  \param command_queue specifies the command-queue being queried.
 *
 *  \param param_name specifies the information to query.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned.  If param_value is
 * NULL, it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.2. If param_value is NULL, it is ignored.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data being queried by param_value. If
 * param_value_size_ret is NULL, it is ignored.
 *
 *  The list of supported param_name values and the information returned in param_value by clGetCommandQueueInfo is
 * described in table 5.2.
 *
 *  \return clGetCommandQueueInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one
 * of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_VALUE if param_name is not one of the supported values or if size in bytes specified by
 * param_value_size is < size of return type as specified in table 5.2 and param_value is not a NULL value.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetCommandQueueInfo)(cl_command_queue command_queue, cl_command_queue_info param_name,
    size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetCommandQueueInfo, "cl_command_queue", command_queue, "cl_command_queue_info",
        param_name, "size_t", param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    return toType<CommandQueue>(command_queue)
        ->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

/*
 * OpenCL 1.0 specification (deprecated in OpenCL 1.1), pages 49+:
 *  Can be used to enable or disable the properties of a command-queue.
 *
 *  \param command_queue specifies the command-queue being queried.
 *
 *  \param properties specifies the new command-queue properties to be applied to command_queue.
 *   Only command-queue properties specified in table 5.1 can be set in properties; otherwise the value specified in
 * properties is considered to be not valid.
 *
 *  \param enable determines whether the values specified by properties are enabled (if enable is CL_TRUE) or disabled
 * (if enable is CL_FALSE) for the command-queue. The property values are described in table 5.1.
 *
 *  \param old_properties returns the command-queue properties before they were changed by clSetCommandQueueProperty.
 * If old_properties is NULL, it is ignored. As specified in table 5.1, the CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE
 * command-queue property determines whether the commands in a command-queue are executed in-order or out-of-order.
 *  Changing this command-queue property will cause the OpenCL implementation to block until all previously queued
 * commands in command_queue have completed. This can be an expensive operation and therefore changes to the
 * CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE property should be only done when absolutely necessary.
 *
 *  \return clSetCommandQueueProperty returns CL_SUCCESS if the command-queue properties are successfully updated.  It
 * returns
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue, returns
 *  - CL_INVALID_VALUE if the values specified in properties are not valid and returns
 *  - CL_INVALID_QUEUE_PROPERTIES if values specified in properties are not supported by the device.
 */
cl_int VC4CL_FUNC(clSetCommandQueueProperty)(cl_command_queue command_queue, cl_command_queue_properties properties,
    cl_bool enable, cl_command_queue_properties* old_properties)
{
    VC4CL_PRINT_API_CALL("cl_int", clSetCommandQueueProperty, "cl_command_queue", command_queue,
        "cl_command_queue_properties", properties, "cl_bool", enable, "cl_command_queue_properties*", old_properties);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    if(old_properties != nullptr)
    {
        cl_int status =
            toType<CommandQueue>(command_queue)
                ->getInfo(CL_QUEUE_PROPERTIES, sizeof(cl_command_queue_properties), old_properties, nullptr);
        if(status != CL_SUCCESS)
            return status;
    }
    return toType<CommandQueue>(command_queue)->setProperties(properties, enable == CL_TRUE);
}

/*!
 * OpenCL 1.2 specification, page 195:
 *
 *  Issues all previously queued OpenCL commands in command_queue to the device associated with command_queue. clFlush
 * only guarantees that all queued commands to command_queue will eventually be submitted to the appropriate device.
 *  There is no guarantee that they will be complete after clFlush returns.
 *
 *  \return clFlush returns CL_SUCCESS if the function call was executed successfully. Otherwise, it returns one of the
 * following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  Any blocking commands queued in a command-queue and clReleaseCommandQueue perform an implicit flush of the
 * command-queue. These blocking commands are clEnqueueReadBuffer, clEnqueueReadBufferRect, clEnqueueReadImage, with
 * blocking_read set to CL_TRUE ; clEnqueueWriteBuffer, clEnqueueWriteBufferRect, clEnqueueWriteImage with
 * blocking_write set to CL_TRUE ; clEnqueueMapBuffer, clEnqueueMapImage with blocking_map set to CL_TRUE ; or
 * clWaitForEvents.
 *
 *  To use event objects that refer to commands enqueued in a command-queue as event objects to wait on by commands
 * enqueued in a different command-queue, the application must call a clFlush or any blocking commands that perform an
 * implicit flush of the command-queue where the commands that refer to these event objects are enqueued.
 */
cl_int VC4CL_FUNC(clFlush)(cl_command_queue command_queue)
{
    VC4CL_PRINT_API_CALL("cl_int", clFlush, "cl_command_queue", command_queue);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    return toType<CommandQueue>(command_queue)->flush();
}

/*!
 * OpenCL 1.2 specification, pages 195+:
 *
 *  Blocks until all previously queued OpenCL commands in command_queue are issued to the associated device and have
 * completed. clFinish does not return until all previously queued commands in command_queue have been processed and
 * completed. clFinish is also a synchronization point.
 *
 *  \return clFinish returns CL_SUCCESS if the function call was executed successfully. Otherwise, it returns one of the
 * following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clFinish)(cl_command_queue command_queue)
{
    VC4CL_PRINT_API_CALL("cl_int", clFinish, "cl_command_queue", command_queue);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    return toType<CommandQueue>(command_queue)->finish();
}
