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

extern cl_int executeKernel(Event* event);

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

static void copy_buffer(const void* in, const size_t in_offset, const size_t size, void* out, const size_t out_offset)
{
	memcpy(out + out_offset, in + in_offset, size);
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

			switch(event->type)
			{
				case CommandType::KERNEL_NDRANGE:
				case CommandType::KERNEL_TASK:
				{
					event->updateStatus(CL_RUNNING);
					//run kernel
					cl_int result = executeKernel(event);
					event->updateStatus(result);
					break;
				}
				case CommandType::KERNEL_NATIVE:
				{
					event->updateStatus(CL_INVALID_OPERATION);
					break;
				}
				case CommandType::MARKER:
				case CommandType::BARRIER:
				{
					//simply complete
					event->updateStatus(CL_RUNNING);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::BUFFER_READ:
				case CommandType::BUFFER_READ_RECT:
				{
					event->updateStatus(CL_RUNNING);
					BufferSource* source = dynamic_cast<BufferSource*>(event->source.get());
					copy_buffer(source->source.buffer->deviceBuffer->hostPointer, source->source.offset, source->source.size, source->dest.hostPtr, source->dest.offset);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::BUFFER_WRITE:
				case CommandType::BUFFER_WRITE_RECT:
				{
					event->updateStatus(CL_RUNNING);
					BufferSource* source = dynamic_cast<BufferSource*>(event->source.get());
					copy_buffer(source->source.hostPtr, source->source.offset, source->source.size, source->dest.buffer->deviceBuffer->hostPointer, source->dest.offset);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::BUFFER_FILL:
				{
					event->updateStatus(CL_RUNNING);
					BufferSource* source = dynamic_cast<BufferSource*>(event->source.get());
					uintptr_t start = (uintptr_t)source->dest.buffer->deviceBuffer->hostPointer;
					uintptr_t end = start + source->source.size;
					while(start < end)
					{
						copy_buffer(source->source.pattern.data(), 0, source->source.pattern.size(), (void*)start, source->dest.offset);
						start += source->source.pattern.size();
					}
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::BUFFER_COPY:
				case CommandType::BUFFER_COPY_RECT:
				{
					event->updateStatus(CL_RUNNING);
					BufferSource* source = dynamic_cast<BufferSource*>(event->source.get());
					void* start = source->source.buffer->deviceBuffer->hostPointer;
					void* dest = source->dest.buffer->deviceBuffer->hostPointer;
					copy_buffer(start, source->source.offset, source->source.size, dest, source->dest.offset);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::BUFFER_MAP:
				{
					//this command doesn't actually do anything, since GPU-memory is always mapped to host-memory
					event->updateStatus(CL_RUNNING);
					dynamic_cast<BufferSource*>(event->source.get())->source.buffer->mappings.push_back(dynamic_cast<BufferSource*>(event->source.get())->dest.hostPtr);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::BUFFER_MIGRATE:
				{
					//this command doesn't actually do anything, since buffers are always located in GPU memory and accessible from host-memory
					event->updateStatus(CL_RUNNING);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::BUFFER_UNMAP:
				{
					//this command doesn't actually do anything, since GPU-memory is always mapped to host-memory
					event->updateStatus(CL_RUNNING);
					BufferSource* source = dynamic_cast<BufferSource*>(event->source.get());
					source->source.buffer->mappings.remove(source->dest.hostPtr);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::SVM_MEMCPY:
				{
					//simply memcpy source.size bytes from source to dest
					event->updateStatus(CL_RUNNING);
					BufferSource* source = dynamic_cast<BufferSource*>(event->source.get());
					copy_buffer(source->source.hostPtr, 0, source->source.size, source->dest.hostPtr, 0);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::SVM_MEMFILL:
				{
					event->updateStatus(CL_RUNNING);
					BufferSource* source = dynamic_cast<BufferSource*>(event->source.get());
					uintptr_t start = (uintptr_t)source->dest.hostPtr;
					uintptr_t end = start + source->source.size;
					while(start < end)
					{
						copy_buffer(source->source.pattern.data(), 0, source->source.pattern.size(), (void*)start, 0);
						start += source->source.pattern.size();
					}
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::SVM_MAP:
				case CommandType::SVM_UNMAP:
				{
					//simply complete
					event->updateStatus(CL_RUNNING);
					event->updateStatus(CL_COMPLETE);
					break;
				}
				case CommandType::SVM_FREE:
				{
					event->updateStatus(CL_RUNNING);
					if(event->source)
					{
						cl_int status = dynamic_cast<CustomSource*>(event->source.get())->func(event);
						if(status != CL_SUCCESS)
							event->updateStatus(status);
						else
							event->updateStatus(CL_COMPLETE);
					}
					else
						event->updateStatus(CL_COMPLETE);
					break;
				}
			}

			//TODO error-handling (via context-pfn_notify) on errors!
			cl_int status = event->release();
			if(status != CL_SUCCESS)
				event->updateStatus(status, false);
			//TODO release event once more?? to destroy it?
			eventProcessed.notify_all();
		}
		else
		{
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
