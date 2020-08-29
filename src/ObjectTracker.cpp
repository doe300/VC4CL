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

BaseObject::~BaseObject() noexcept = default;

static ObjectTracker liveObjectsTracker;

ObjectTracker::~ObjectTracker()
{
    // since this is called at the end of the program,
    // all objects still alive here are leaked!
    if(!liveObjects.empty())
    {
        DEBUG_LOG(DebugLevel::OBJECTS, {
            for(const auto& obj : liveObjects)
            {
                std::cout << "[VC4CL] Leaked object " << obj->getBasePointer() << " with " << obj->referenceCount
                          << " references: " << obj->typeName << "\n";
            }
            std::cout << std::endl;
        })
    }

    // the remaining objects reference one another
    // since deleting one object may remove another, we cannot use the cleanup-function of the container's destructor
    // also, we cannot simply delete all objects in order (some might not exist anymore)
    while(!liveObjects.empty())
    {
        // we delete from the back, since the first objects are usually things like device, context, etc. and still
        // referenced by the other objects (since we insert our pointers in the order of creation)
        liveObjects.erase(--liveObjects.end());
    }
}

void ObjectTracker::addObject(BaseObject* obj)
{
    std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);
    liveObjectsTracker.liveObjects.emplace_back(obj);
    DEBUG_LOG(DebugLevel::OBJECTS,
        std::cout << "Tracking live-time of object: " << obj->getBasePointer() << " (" << obj->typeName << ')'
                  << std::endl)
}

void ObjectTracker::removeObject(BaseObject* obj)
{
    std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);
    DEBUG_LOG(DebugLevel::OBJECTS,
        std::cout << "Releasing live-time of object: " << obj->getBasePointer() << " (" << obj->typeName << ')'
                  << std::endl)
    // TODO since the short-lived objects tend to be located at the end, we should search in reverse direction!
    auto it = std::find_if(liveObjectsTracker.liveObjects.begin(), liveObjectsTracker.liveObjects.end(),
        [obj](const std::unique_ptr<BaseObject>& ptr) -> bool { return ptr.get() == obj; });
    if(it != liveObjectsTracker.liveObjects.end())
        liveObjectsTracker.liveObjects.erase(it);
    else
        DEBUG_LOG(DebugLevel::OBJECTS,
            std::cout << "Removing object not previously tracked: " << obj->getBasePointer() << " (" << obj->typeName
                      << ')' << std::endl)
}

void ObjectTracker::iterateObjects(ReportFunction func, void* userData)
{
    std::lock_guard<std::recursive_mutex> guard(trackerMutex);

    for(const auto& obj : liveObjects)
    {
        func(userData, obj->getBasePointer(), obj->typeName, obj->referenceCount);
    }
}

const BaseObject* ObjectTracker::findTrackedObject(const std::function<bool(const BaseObject&)>& predicate)
{
    std::lock_guard<std::recursive_mutex> guard(liveObjectsTracker.trackerMutex);

    for(const auto& obj : liveObjectsTracker.liveObjects)
    {
        if(predicate(*obj))
            return obj.get();
    }
    return nullptr;
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
        "void(CL_CALLBACK*)(void*, void*, const char*, cl_uint)", &report_fn, "void*", user_data);
    liveObjectsTracker.iterateObjects(report_fn, user_data);
}
