/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef TYPES_H
#define TYPES_H

#include "icd_loader.h"
#include "vc4cl_config.h"

#include <cassert>
#include <type_traits>

struct _cl_object
{
    vc4cl_icd_dispatch void* object;

    explicit _cl_object(void* obj) noexcept : object(obj)
    {
        static_assert(!std::is_copy_constructible<_cl_object>::value, "VC4CL objects can't be copied!");
        static_assert(!std::is_move_constructible<_cl_object>::value, "VC4CL objects can't be moved!");
        static_assert(!std::is_copy_assignable<_cl_object>::value, "VC4CL objects can't be copied!");
        static_assert(!std::is_move_assignable<_cl_object>::value, "VC4CL objects can't be moved!");
    }

    _cl_object(const _cl_object&) = delete;
    _cl_object(_cl_object&&) = delete;
    ~_cl_object() noexcept = default;

    _cl_object& operator=(const _cl_object&) = delete;
    _cl_object& operator=(_cl_object&&) = delete;
};

struct _cl_platform_id : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_platform_id";

    explicit _cl_platform_id(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_platform_id>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(
            offsetof(_cl_platform_id, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

struct _cl_device_id : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_device_id";

    explicit _cl_device_id(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_device_id>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(offsetof(_cl_device_id, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

struct _cl_context : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_context";

    explicit _cl_context(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_context>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(offsetof(_cl_context, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

/*!
 * OpenCL 1.2 specification, page 62:
 *  The command-queue can be used to queue a set of operations (referred to as commands) in order.
 *  Having multiple command-queues allows applications to queue multiple independent commands without requiring
 * synchronization. Note that this should work as long as these objects are not being shared. Sharing of objects across
 * multiple command-queues will require the application to perform appropriate synchronization.
 */
struct _cl_command_queue : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_command_queue";

    explicit _cl_command_queue(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_command_queue>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(
            offsetof(_cl_command_queue, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

/*!
 * OpenCL 1.2 specification, page 67:
 *  A buffer object stores a one-dimensional collection of elements.
 *  Elements of a buffer object can be a scalar data type (such as an int, float), vector data type, or a user-defined
 * structure.
 */
struct _cl_mem : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_mem";

    explicit _cl_mem(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_mem>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(offsetof(_cl_mem, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

/*!
 * OpenCL 1.2 specification, page 179:
 *
 *  An event object can be used to track the execution status of a command. The API calls that enqueue commands to a
 * command-queue create a new event object that is returned in the event argument. In case of an error enqueuing the
 * command in the command-queue the event argument does not return an event object. The execution status of an enqueued
 * command at any given point in time can be one of the following:
 *  - CL_QUEUED – This indicates that the command has been enqueued in a command-queue. This is the initial state of all
 * events except user events.
 *  - CL_SUBMITTED – This is the initial state for all user events. For all other events, this indicates that the
 * command has been submitted by the host to the device.
 *  - CL_RUNNING – This indicates that the device has started executing this command. In order for the execution status
 * of an enqueued command to change from CL_SUBMITTED to CL_RUNNING , all events that this command is waiting on must
 * have completed successfully i.e. their execution status must be CL_COMPLETE .
 *  - CL_COMPLETE – This indicates that the command has successfully completed.
 *  - Error code – The error code is a negative integer value and indicates that the command was abnormally terminated.
 * Abnormal termination may occur for a number of reasons such as a bad memory access.
 *
 *  NOTE: A command is considered to be complete if its execution status is CL_COMPLETE or is a negative integer value.
 */
struct _cl_event : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_event";

    explicit _cl_event(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_event>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(offsetof(_cl_event, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

/*!
 * OpenCL 1.2 specification, page 133:
 *
 *  An OpenCL program consists of a set of kernels that are identified as functions declared with the __kernel qualifier
 * in the program source. OpenCL programs may also contain auxiliary functions and constant data that can be used by
 * __kernel functions. The program executable can be generated online or offline by the OpenCL compiler for the
 * appropriate target device(s).
 *
 *  A program object encapsulates the following information:
 *  - An associated context.
 *  - A program source or binary.
 *  - The latest successfully built program executable, library or compiled binary, the list of devices for which the
 * program executable, library or compiled binary is built, the build options used and a build log.
 *  - The number of kernel objects currently attached.
 */
struct _cl_program : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_program";

    explicit _cl_program(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_program>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(offsetof(_cl_program, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

/*!
 * OpenCL 1.2 specification, page 158:
 *
 *  A kernel is a function declared in a program. A kernel is identified by the __kernel qualifier applied to any
 * function in a program. A kernel object encapsulates the specific __kernel function declared in a program and the
 * argument values to be used when executing this __kernel function.
 */
struct _cl_kernel : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_kernel";

    explicit _cl_kernel(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_kernel>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(offsetof(_cl_kernel, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

/*!
 * OpenCL 1.2 specification, page 129:
 *
 *  A sampler object describes how to sample an image when the image is read in the kernel. The built-in functions to
 * read from an image in a kernel take a sampler as an argument. The sampler arguments to the image read function can be
 * sampler objects created using OpenCL functions and passed as argument values to the kernel or can be samplers
 * declared inside a kernel.
 */
struct _cl_sampler : public _cl_object
{
    constexpr static const char* TYPE_NAME = "cl_sampler";

    explicit _cl_sampler(void* obj) : _cl_object(obj)
    {
        static_assert(std::is_standard_layout<_cl_sampler>::value,
            "This is required for the ICD-loader to correctly find the dispatcher");
#if use_cl_khr_icd
        static_assert(offsetof(_cl_sampler, dispatch) == 0, "The ICD dispatch-object is required to have no offset!");
        assert(dispatch != nullptr);
#endif
        assert(object != nullptr);
    }
};

#endif /* TYPES_H */
