#pragma once

#include "common.h"

#include <functional>

namespace vc4cl
{
    /**
     * Handle to the actual kernel code execution
     *
     * Using this handle type allows for non-blocking execution and provides a way to wait for the execution to finish
     * and query its status.
     */
    class ExecutionHandle
    {
        /**
         * Container for the different states of a kernel execution
         */
        enum class ExecutionState
        {
            // The execution is not yet finished
            PENDING,
            // The execution finished successfully
            PASSED,
            // The execution failed, e.g. it timed out
            FAILED
        };

    public:
        explicit ExecutionHandle(std::function<bool()>&& func) : func(std::move(func)), status(ExecutionState::PENDING)
        {
        }
        explicit ExecutionHandle(bool success) :
            func{}, status(success ? ExecutionState::PASSED : ExecutionState::FAILED)
        {
        }
        ExecutionHandle(const ExecutionHandle&) = delete;
        ExecutionHandle(ExecutionHandle&&) = default;
        ~ExecutionHandle() = default;

        ExecutionHandle& operator=(const ExecutionHandle&) = delete;
        ExecutionHandle& operator=(ExecutionHandle&&) = default;

        /**
         * Blocks the calling thread until the execution finishes and returns the success state
         *
         * @return whether or not the execution was successful.
         */
        inline CHECK_RETURN bool waitFor()
        {
            if(status != ExecutionState::PENDING)
                return status == ExecutionState::PASSED;
            bool success = func();
            status = success ? ExecutionState::PASSED : ExecutionState::FAILED;
            return success;
        }

        inline ExecutionState getStatus() const noexcept
        {
            return status;
        }

    private:
        std::function<bool()> func;
        ExecutionState status;
    };
} // namespace vc4cl
