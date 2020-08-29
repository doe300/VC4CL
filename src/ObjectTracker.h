/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_OBJECT_TRACKER_H
#define VC4CL_OBJECT_TRACKER_H

#include "types.h"

#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <list>

namespace vc4cl
{
    class BaseObject;
    /*
     * Tracks all live OpenCL objects
     */
    class ObjectTracker
    {
        using ReportFunction = void(CL_CALLBACK*)(void* userData, void* objPtr, const char* typeName, cl_uint refCount);

    public:
        ~ObjectTracker();

        static void addObject(BaseObject* obj);
        static void removeObject(BaseObject* obj);

        void iterateObjects(ReportFunction func, void* userData);

        /**
         * Tries to find the VC4CL object matching the given criteria
         */
        static const BaseObject* findTrackedObject(const std::function<bool(const BaseObject&)>& predicate);

    private:
        std::list<std::unique_ptr<BaseObject>> liveObjects;
        // recursive-mutex required, since a #removeObject() can cause #removeObject() to be called multiple times (e.g.
        // for last CommandQueue also releasing the Context)
        std::recursive_mutex trackerMutex;
    };
} /* namespace vc4cl */

#endif /* VC4CL_OBJECT_TRACKER_H */
