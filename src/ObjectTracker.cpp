/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "ObjectTracker.h"
#include "Object.h"

#include <algorithm>
#include <iostream>

using namespace vc4cl;

static ObjectTracker liveObjectsTracker;

ObjectTracker::~ObjectTracker()
{
	//since this is called at the end of the program,
	//all objects still alive here are leaked!
#ifdef DEBUG_MODE
	for(const auto& obj : liveObjects)
	{
		std::cout << "[VC4CL] Leaked object: " << obj->typeName << "\n";
	}
	std::cout << std::endl;
#endif
}

void ObjectTracker::addObject(ParentObject* obj)
{
	std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);
	liveObjectsTracker.liveObjects.emplace(obj);
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Tracking live-time of object: " << obj->typeName << std::endl;
#endif
}

void ObjectTracker::removeObject(ParentObject* obj)
{
	std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);
#ifdef DEBUG_MODE
	std::cout << "[VC4CL] Releasing live-time of object: " << obj->typeName << std::endl;
#endif
	auto it = std::find_if(liveObjectsTracker.liveObjects.begin(), liveObjectsTracker.liveObjects.end(), [obj](const std::unique_ptr<ParentObject>& ptr) -> bool
	{
		return ptr.get() == obj;
	});
	if(it != liveObjectsTracker.liveObjects.end())
		liveObjectsTracker.liveObjects.erase(it);
#ifdef DEBUG_MODE
	else
		std::cout << "[VC4CL] Removing object not previously tracked: " << obj->typeName << std::endl;
#endif
}
