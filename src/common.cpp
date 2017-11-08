/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include <CL/opencl.h>
#include <string.h>
#include <stdlib.h>
#include <sstream>

#include "common.h"

using namespace vc4cl;

std::string vc4cl::joinStrings(const std::vector<std::string>& strings, const std::string& delim)
{
	std::stringstream s;
	for(size_t i = 0; i < strings.size(); ++i)
	{
	  if(i > 0)
	    s << delim;
	  s << strings.at(i);
	}
	return s.str();
}

cl_int vc4cl::returnValue(const void* value, const size_t value_size, const size_t value_count, size_t output_size, void* output, size_t* output_size_ret)
{
	if(output != nullptr)
	{
		if(output_size < value_size * value_count)
			//not enough space on output parameter
			return CL_INVALID_VALUE;
		memcpy(output, value, value_size * value_count);
	}
	if(output_size_ret != nullptr)
		*output_size_ret = value_count * value_size;
	return CL_SUCCESS;
}

cl_int vc4cl::returnString(const std::string& string, size_t output_size, void* output, size_t* output_size_ret)
{
	const size_t string_length = string.length() + 1 /* 0-byte */;
	return returnValue(string.data(), sizeof(char), string_length, output_size, output, output_size_ret);
}
