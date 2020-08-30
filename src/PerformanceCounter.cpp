/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "PerformanceCounter.h"

#include "Device.h"
#include "V3D.h"

#include <bitset>
#include <memory>
#include <mutex>

using namespace vc4cl;

static std::mutex counterAccessMutex;
static std::bitset<16> usedCounters{0};

PerformanceCounter::PerformanceCounter(cl_counter_type_vc4cl type, cl_uchar index) : type(type), index(index)
{
    // no need to lock, lock is already held by clCreatePerformanceCounterVC4CL
    if(!V3D::instance()->setCounter(index, static_cast<vc4cl::CounterType>(type)))
    {
        // all error-cases care checked before
        throw std::invalid_argument("Failed to set counter configuration!");
    }
}

PerformanceCounter::~PerformanceCounter()
{
    std::lock_guard<std::mutex> lock(counterAccessMutex);
    V3D::instance()->disableCounter(index);
    usedCounters.reset(index);
}

cl_int PerformanceCounter::getValue(cl_uint* value) const
{
    if(value == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameter is NULL!");
    std::lock_guard<std::mutex> lock(counterAccessMutex);
    *value = static_cast<cl_uint>(V3D::instance()->getCounter(index));
    return CL_SUCCESS;
}

cl_int PerformanceCounter::reset()
{
    std::lock_guard<std::mutex> lock(counterAccessMutex);
    V3D::instance()->resetCounterValue(index);
    return CL_SUCCESS;
}

cl_counter_vc4cl VC4CL_FUNC(clCreatePerformanceCounterVC4CL)(
    cl_device_id device, const cl_counter_type_vc4cl counter_type, cl_int* errcode_ret)
{
    std::lock_guard<std::mutex> lock(counterAccessMutex);
    CHECK_DEVICE_ERROR_CODE(toType<Device>(device), errcode_ret, cl_counter_vc4cl)

    if(counter_type > 29)
        return returnError<cl_counter_vc4cl>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, buildString("Invalid counter-type %u!", counter_type));
    if(usedCounters.all())
        return returnError<cl_counter_vc4cl>(
            CL_OUT_OF_RESOURCES, errcode_ret, __FILE__, __LINE__, "No more free counters!");

    cl_uchar counterIndex = 0;
    for(; counterIndex < usedCounters.size(); ++counterIndex)
    {
        if(!usedCounters.test(counterIndex))
            break;
    }
    usedCounters.set(counterIndex);

    PerformanceCounter* counter = newOpenCLObject<PerformanceCounter>(counter_type, counterIndex);
    CHECK_ALLOCATION_ERROR_CODE(counter, errcode_ret, cl_counter_vc4cl)
    RETURN_OBJECT(counter->toBase(), errcode_ret)
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
