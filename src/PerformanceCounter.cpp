/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <memory>
#include <array>

#include "PerformanceCounter.h"
#include "Device.h"
#include "V3D.h"

using namespace vc4cl;

PerformanceCounter::PerformanceCounter(cl_counter_type_vc4cl type, cl_uchar index) : type(type), index(index)
{
}

PerformanceCounter::~PerformanceCounter()
{
	V3D::instance().disableCounter(index);
}

cl_int PerformanceCounter::getValue(cl_uint* value) const
{
	if(value == nullptr)
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameter is NULL!");
	*value = V3D::instance().getCounter(index);
	return CL_SUCCESS;
}

cl_int PerformanceCounter::reset()
{
	V3D::instance().resetCounterValue(index);
	return CL_SUCCESS;
}

static std::array<std::unique_ptr<PerformanceCounter>, 16> counters;

cl_counter_vc4cl VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(cl_device_id device, const cl_counter_type_vc4cl counter_type, cl_int* errcode_ret)
{
	CHECK_DEVICE_ERROR_CODE(toType<Device>(device), errcode_ret, cl_counter_vc4cl)

	if(counter_type > 29)
		return returnError<cl_counter_vc4cl>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, buildString("Invalid counter-type %u!", counter_type));

	cl_char counter_index = -1;
	for(cl_char i = 0; i < counters.size(); ++i)
	{
		if(counters[i].get() == nullptr)
		{
			counter_index = i;
			break;
		}
	}
	if(counter_index < 0)
		return returnError<cl_counter_vc4cl>(CL_OUT_OF_RESOURCES, errcode_ret, __FILE__, __LINE__, "No more free counters!");

	counters[counter_index].reset(newObject<PerformanceCounter>(counter_type, counter_index));
	CHECK_ALLOCATION_ERROR_CODE(counters[counter_index].get(), errcode_ret, cl_counter_vc4cl)
	if(V3D::instance().setCounter(counter_index, static_cast<vc4cl::CounterType>(counter_type)) != 0)
	{
		counters[counter_index].reset();
		return returnError<cl_counter_vc4cl>(CL_OUT_OF_RESOURCES, errcode_ret, __FILE__, __LINE__, "Failed to set counter configuration!");
	}

	RETURN_OBJECT(counters[counter_index]->toBase(), errcode_ret)
}

cl_int VC4CL_FUNC(clGetPerformanceCounterValueVC4CL)(cl_counter_vc4cl counter, cl_uint* value)
{
	CHECK_COUNTER(toType<PerformanceCounter>(counter))
	return toType<PerformanceCounter>(counter)->getValue(value);
}

cl_int VC4CL_FUNC(clReleasePerformanceCounterVC4CL)(cl_counter_vc4cl counter)
{
	CHECK_COUNTER(toType<PerformanceCounter>(counter))
	return toType<PerformanceCounter>(counter)->release();
}

cl_int VC4CL_FUNC(clRetainPerformanceCounterVC4CL)(cl_counter_vc4cl counter)
{
	CHECK_COUNTER(toType<PerformanceCounter>(counter))
	return toType<PerformanceCounter>(counter)->retain();
}

cl_int VC4CL_FUNC(clResetPerformanceCounterValueVC4CL)(cl_counter_vc4cl counter)
{
	CHECK_COUNTER(toType<PerformanceCounter>(counter))
	return toType<PerformanceCounter>(counter)->reset();
}
