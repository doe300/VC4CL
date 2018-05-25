/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "ObjectTracker.h"
#include "Object.h"
#include "extensions.h"

#include <algorithm>
#include <iostream>

using namespace vc4cl;

static ObjectTracker liveObjectsTracker;

ObjectTracker::~ObjectTracker()
{
// since this is called at the end of the program,
// all objects still alive here are leaked!
#ifdef DEBUG_MODE
    for(const auto& obj : liveObjects)
    {
        std::cout << "[VC4CL] Leaked object with " << obj->referenceCount << " references: " << obj->typeName << "\n";
    }
    std::cout << std::endl;
#endif

    // the remaining objects reference one another
    // since deleting one object may remove another, we cannot use the cleanup-function of the container's destructor
    // also, we cannot simply delete all objects in order (some might not exist anymore)
    while(!liveObjects.empty())
    {
        liveObjects.erase(liveObjects.begin());
    }
}

void ObjectTracker::addObject(BaseObject* obj)
{
    std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);
    liveObjectsTracker.liveObjects.emplace(obj);
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Tracking live-time of object: " << obj->typeName << std::endl;
#endif
}

void ObjectTracker::removeObject(BaseObject* obj)
{
    std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);
#ifdef DEBUG_MODE
    std::cout << "[VC4CL] Releasing live-time of object: " << obj->typeName << std::endl;
#endif
    auto it = std::find_if(liveObjectsTracker.liveObjects.begin(), liveObjectsTracker.liveObjects.end(),
        [obj](const std::unique_ptr<BaseObject>& ptr) -> bool { return ptr.get() == obj; });
    if(it != liveObjectsTracker.liveObjects.end())
        liveObjectsTracker.liveObjects.erase(it);
#ifdef DEBUG_MODE
    else
        std::cout << "[VC4CL] Removing object not previously tracked: " << obj->typeName << std::endl;
#endif
}

void ObjectTracker::iterateObjects(ReportFunction func, void* userData)
{
    std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);

    for(const auto& obj : liveObjects)
    {
        func(userData, obj->getBasePointer(), obj->typeName, obj->referenceCount);
    }
}

void VC4CL_FUNC(clTrackLiveObjectsAltera)(cl_platform_id platform)
{
    VC4CL_PRINT_API_CALL("void", clTrackLiveObjectsAltera, "cl_platform_id", platform);
    // no-op, object tracking is always enabled
}

void VC4CL_FUNC(clReportLiveObjectsAltera)(cl_platform_id platform,
    void(CL_CALLBACK* report_fn)(
        void* /* user_data */, void* /* obj_ptr */, const char* /* type_name */, cl_uint /* refcount */),
    void* user_data)
{
    VC4CL_PRINT_API_CALL("void", clReportLiveObjectsAltera, "cl_platform_id", platform,
        "void(CL_CALLBACK*)(void*, void*, const char*, cl_uint)", report_fn, "void*", user_data);
    liveObjectsTracker.iterateObjects(report_fn, user_data);
}
