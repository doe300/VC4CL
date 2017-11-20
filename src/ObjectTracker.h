/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_OBJECT_TRACKER_H
#define VC4CL_OBJECT_TRACKER_H

#include "types.h"

#include <memory>
#include <mutex>
#include <set>

namespace vc4cl
{
	class ParentObject;
	/*
	 * Tracks all live OpenCL objects
	 */
	class ObjectTracker
	{
	public:
		~ObjectTracker();

		static void addObject(ParentObject* obj);
		static void removeObject(ParentObject* obj);
	private:
		std::set<std::unique_ptr<ParentObject>> liveObjects;
		std::mutex trackerMutex;
	};

} /* namespace vc4cl */

#endif /* VC4CL_OBJECT_TRACKER_H */
