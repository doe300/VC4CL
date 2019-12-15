/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_QUEUEHANDLER
#define VC4CL_QUEUEHANDLER

#include "Event.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace vc4cl
{
    class CommandQueue;

    /**
     * Singleton handler of the actual event executions
     */
    class EventQueue
    {
    public:
        EventQueue(const EventQueue&) = delete;
        EventQueue(EventQueue&&) noexcept = delete;
        ~EventQueue() noexcept;

        EventQueue& operator=(const EventQueue&) = delete;
        EventQueue& operator=(EventQueue&&) noexcept = delete;

        /**
         * Enqueues a new event to be executed in this event queue
         */
        void pushEvent(Event* event);

        /**
         * Returns the first (oldest) event scheduled via the given command queue, if any such event exists.
         */
        object_wrapper<Event> peek(CommandQueue* queue);

        /**
         * Blocks the caller until the given event has finished execution.
         */
        void waitForEvent(const Event* event);

        static std::shared_ptr<EventQueue> getInstance();

    private:
        EventQueue();

        std::atomic_bool continueRunning;

        std::deque<object_wrapper<Event>> eventBuffer{};
        // this is triggered after every finished cl_event
        std::condition_variable eventProcessed{};
        // this is triggered if a new event is available
        std::condition_variable eventAvailable{};
        std::mutex listenMutex{};
        std::mutex bufferMutex{};
        std::mutex eventMutex{};

        // the actual thread needs to be initialized after all the mutices
        std::thread eventHandler;

        Event* peekQueue();
        void popFromEventQueue();

        void runEventQueue();
    };
} /* namespace vc4cl */

#endif /* VC4CL_QUEUEHANDLER */
