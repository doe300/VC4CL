/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "queue_handler.h"

#include "Buffer.h"
#include "Event.h"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <sys/prctl.h>
#include <thread>

using namespace vc4cl;

static std::deque<Event*> eventBuffer;

static const std::chrono::steady_clock::duration WAIT_DURATION = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(10));
//this is triggered after every finished cl_event
static std::condition_variable eventProcessed;
//this is triggered if a new event is available
static std::condition_variable eventAvailable;
static std::mutex listenMutex;
static std::mutex bufferMutex;
static std::mutex eventMutex;

static std::mutex queuesMutex;
static std::thread eventHandler;
static cl_uint numCommandQueues;

void vc4cl::pushEventToQueue(Event* event)
{
	std::lock_guard<std::mutex> guard(bufferMutex);
	eventBuffer.push_back(event);
	eventAvailable.notify_all();
}

Event* popEventFromQueue()
{
	std::lock_guard<std::mutex> guard(bufferMutex);
	if(eventBuffer.empty())
		return nullptr;
	Event* event = eventBuffer.front();
	eventBuffer.pop_front();
	return event;
}

Event* vc4cl::peekQueue(CommandQueue* queue)
{
	std::lock_guard<std::mutex> guard(bufferMutex);

	for(Event* e : eventBuffer)
	{
		if(e->getCommandQueue() == queue)
			return e;
	}
	return nullptr;
}

void vc4cl::waitForEvent(const Event* event)
{
	std::unique_lock<std::mutex> lock(listenMutex);

	while(!event->isFinished())
	{
		//lock on some semaphore triggered when done
		//this is triggered for every event, so re-check for this particular event
		eventProcessed.wait(lock);
	}
}

static void runEventQueue()
{
	//Sets the POSIX thread name
	prctl(PR_SET_NAME, "VC4CL Queue Handler", 0, 0, 0);
	while(numCommandQueues != 0)
	{
		Event* event = popEventFromQueue();
		if(event != nullptr)
		{
			event->updateStatus(CL_SUBMITTED);
			event->updateStatus(CL_RUNNING);
			if(event->action)
			{
				cl_int status = event->action->operator()(event);
				if(status != CL_SUCCESS)
					event->updateStatus(status);
				else
					event->updateStatus(CL_COMPLETE);
			}
			else
				event->updateStatus(returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "No event source specified!"));

			//TODO error-handling (via context-pfn_notify) on errors? Neither PoCL nor beignet seem to use context's pfn_notify
			cl_int status = event->release();
			if(status != CL_SUCCESS)
				event->updateStatus(status, false);
			eventProcessed.notify_all();
		}
		else
		{
			std::unique_lock<std::mutex> lock(eventMutex);
			//sometimes locks infinite (race condition on event set after the check above but before the wait()?)
			//-> for now, simply wait for a maximum amount of time and check again
			eventAvailable.wait_for(lock, WAIT_DURATION);
			eventProcessed.notify_all();
		}
	}
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Queue handler thread stopped" << std::endl;
#endif
}

void vc4cl::initEventQueue()
{
	std::lock_guard<std::mutex> guard(queuesMutex);
	numCommandQueues++;
	if(!eventHandler.joinable())
	{
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Starting queue handler thread..." << std::endl;
#endif
		eventHandler = std::thread(runEventQueue);
	}
}

void vc4cl::deinitEventQueue()
{
	std::lock_guard<std::mutex> guard(queuesMutex);
	numCommandQueues--;
	if(numCommandQueues == 0 && eventHandler.joinable())
	{
#ifdef DEBUG_MODE
		std::cout << "[VC4CL] Stopping queue handler thread..." << std::endl;
#endif
		//wake up event handler, so we can stop it
		eventAvailable.notify_all();
		eventHandler.join();
	}
}
