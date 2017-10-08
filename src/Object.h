/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_OBJECT_H
#define VC4CL_OBJECT_H

#include <CL/opencl.h>
#include <string>
#include <string.h>

#include "common.h"

namespace vc4cl
{
	template<typename BaseType, cl_int invalidObjectCode>
	class Object
	{
	public:

		virtual ~Object()
		{
			//required, so the destructors of the child-classes are called
		}

		CHECK_RETURN cl_int retain()
		{
			if(!checkReferences())
				return returnError(invalidObjectCode, __FILE__, __LINE__, "Object reference check failed!");
			referenceCount += 1;
			return CL_SUCCESS;
		}

		CHECK_RETURN cl_int release()
		{
			if(!checkReferences())
				return returnError(invalidObjectCode, __FILE__, __LINE__, "Object reference check failed!");
			referenceCount -= 1;
			if(referenceCount == 0)
				delete this;
			return CL_SUCCESS;
		}

		BaseType* toBase()
		{
			return &base;
		}

		const BaseType* toBase() const
		{
			return &base;
		}

		inline bool checkReferences() const
		{
			return referenceCount > 0;
		}

		inline cl_uint getReferences() const
		{
			return referenceCount;
		}

	protected:

		BaseType base;
		cl_uint referenceCount;

		Object() : base(this), referenceCount(1)
		{
			//reference-count is implicitly retained
		}

	private:
		//make sure, objects can't be copied or moved, since it invalidates the pointers
		Object(const Object&) = delete;
		Object(Object&&) = delete;

		Object& operator=(const Object&) = delete;
		Object& operator=(Object&&) = delete;
	};
};

#endif /* VC4CL_OBJECT_H */
