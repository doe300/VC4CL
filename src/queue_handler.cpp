/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "queue_handler.h"
#include <queue>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <sys/prctl.h>

#include "Event.h"
#include "Buffer.h"

using namespace vc4cl;

static std::deque<Event*> eventBuffer;

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

			//TODO error-handling (via context-pfn_notify) on errors!
			cl_int status = event->release();
			if(status != CL_SUCCESS)
				event->updateStatus(status, false);
			//TODO release event once more?? to destroy it?
			eventProcessed.notify_all();
		}
		else
		{
			//TODO sometimes locks infinite (race condition on event set after the check above but before the wait()?)
			std::unique_lock<std::mutex> lock(eventMutex);
			eventAvailable.wait(lock);
			eventProcessed.notify_all();
		}
	}
}

void vc4cl::initEventQueue()
{
	std::lock_guard<std::mutex> guard(queuesMutex);
	numCommandQueues++;
	if(!eventHandler.joinable())
	{
		eventHandler = std::thread(runEventQueue);
	}
}

void vc4cl::deinitEventQueue()
{
	std::lock_guard<std::mutex> guard(queuesMutex);
	numCommandQueues--;
	if(numCommandQueues == 0 && eventHandler.joinable())
	{
		//wake up event handler, so we can stop it
		eventAvailable.notify_all();
		eventHandler.join();
	}
}
