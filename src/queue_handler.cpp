/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "queue_handler.h"

#include "Buffer.h"
#include "Event.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <sys/prctl.h>
#include <thread>

using namespace vc4cl;

static const std::chrono::steady_clock::duration WAIT_DURATION =
    std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(10));

EventQueue::EventQueue() : continueRunning(true), eventHandler(std::bind(&EventQueue::runEventQueue, this))
{
    DEBUG_LOG(DebugLevel::EVENTS, std::cout << "Starting queue handler thread..." << std::endl);
}

EventQueue::~EventQueue() noexcept
{
    DEBUG_LOG(DebugLevel::EVENTS, std::cout << "Stopping queue handler thread..." << std::endl)
    continueRunning = false;
    // wake up event handler, so we can stop it
    eventAvailable.notify_all();
    eventHandler.join();
}

void EventQueue::pushEvent(Event* event)
{
    std::lock_guard<std::mutex> guard(bufferMutex);
    eventBuffer.emplace_back(object_wrapper<Event>{event});
    eventAvailable.notify_all();
}

Event* EventQueue::peekQueue()
{
    std::lock_guard<std::mutex> guard(bufferMutex);
    if(eventBuffer.empty())
        return nullptr;
    return eventBuffer.front().get();
}

void EventQueue::popFromEventQueue()
{
    std::lock_guard<std::mutex> guard(bufferMutex);
    if(eventBuffer.empty())
        return;
    eventBuffer.pop_front();
}

object_wrapper<Event> EventQueue::peek(CommandQueue* queue)
{
    std::lock_guard<std::mutex> guard(bufferMutex);

    for(auto& e : eventBuffer)
    {
        if(e->getCommandQueue() == queue)
            return e;
    }
    return {};
}

void EventQueue::waitForEvent(const Event* event)
{
    std::unique_lock<std::mutex> lock(listenMutex);

    while(!event->isFinished())
    {
        // lock on some semaphore triggered when done
        // this is triggered for every event, so re-check for this particular event
        eventProcessed.wait(lock);
    }
}

std::shared_ptr<EventQueue> EventQueue::getInstance()
{
    // use a shared_ptr to make sure, the thread is only cleaned up after all command queues accessing it are deleted.
    static std::shared_ptr<EventQueue> queue(new EventQueue());
    return queue;
}

void EventQueue::runEventQueue()
{
    // Sets the POSIX thread name
    prctl(PR_SET_NAME, "VC4CL Queue Handler", 0, 0, 0);
    while(continueRunning)
    {
        Event* event = peekQueue();
        if(event)
            event->updateStatus(CL_SUBMITTED);
        /*
         * Usually, all events in a wait-list are enqueued and therefore finished before this event is executed.
         * There is an exception though if the event waits for a user-event which is never enqueued!
         * So we need to check the wait list whether it is actually all finished.
         *
         * OpenCL 1.2 specification, section 5.9:
         * "In order for the execution status of an enqueued command to change from CL_SUBMITTED to CL_RUNNING , all
         * events that this command is waiting on must have completed successfully i.e. their execution status must be
         * CL_COMPLETE."
         * -> we need to check for wait list between CL_SUBMITTED and CL_RUNNING
         *
         * It is the applications responsibility to avoid deadlocks when using user-events, see NOTE on OpenCL 1.2,
         * section 5.9 paragraph for 'clReleaseEvent'.
         *
         * FIXME waiting events block the queue. Can we skip them and continue with the next one?
         * We could at least do that for out-of-order queues or if the next event is from a different queue.
         * See OpenCL 1.2, section 5.11
         */
        WaitListStatus waitListStatus = WaitListStatus::PENDING;
        if(event && ((waitListStatus = event->getWaitListStatus()) != WaitListStatus::PENDING))
        {
            if(waitListStatus == WaitListStatus::ERROR)
            {
                // at least one event in the wait list had an error, so we abort this execution
                event->updateStatus(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST);
            }
            else
            {
                event->updateStatus(CL_RUNNING);
                if(event->action)
                {
                    cl_int status = event->action->operator()();
                    if(status != CL_SUCCESS)
                        event->updateStatus(status);
                    else
                        event->updateStatus(CL_COMPLETE);
                }
                else
                    event->updateStatus(
                        returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "No event source specified!"));

                // TODO error-handling (via context-pfn_notify) on errors? Neither PoCL nor beignet seem to use
                // context's pfn_notify
                cl_int status = event->release();
                if(status != CL_SUCCESS)
                    event->updateStatus(status, false);
            }
            // clear the wait list of this event to allow resources of waited-for events to be released before we
            // release this event itself.
            event->clearWaitList();
            eventProcessed.notify_all();
            // we need to leave the event in the queue until it is finished processing to allow CommandQueue#finish() to
            // track it
            popFromEventQueue();
        }
        else
        {
            std::unique_lock<std::mutex> lock(eventMutex);
            // sometimes locks infinite (race condition on event set after the check above but before the wait()?)
            //-> for now, simply wait for a maximum amount of time and check again
            // Also, this waits for an event's wait-list to become finished (esp. user events) and therefore should not
            // wait forever but must wake up in small intervals!
            eventAvailable.wait_for(lock, WAIT_DURATION);
            eventProcessed.notify_all();
        }
    }
    DEBUG_LOG(DebugLevel::EVENTS, std::cout << "Queue handler thread stopped" << std::endl)
}
