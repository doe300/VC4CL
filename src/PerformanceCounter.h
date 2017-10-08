/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef PERFORMANCE_COUNTER_H
#define PERFORMANCE_COUNTER_H

#include "extensions.h"

#include "Object.h"
#include "Context.h"

namespace vc4cl
{
	class PerformanceCounter: public Object<_cl_counter_vc4cl, CL_INVALID_PERFORMANCE_COUNTER>
	{
	public:
		PerformanceCounter(cl_counter_type_vc4cl type, cl_uint index);
		~PerformanceCounter();

		cl_int getValue(cl_uint* value) const;
		cl_int reset();

	private:
		cl_counter_type_vc4cl type;
		cl_uint index;
	};

} /* namespace vc4cl */

#endif /* PERFORMANCE_COUNTER_H */
