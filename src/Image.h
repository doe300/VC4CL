/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_IMAGE
#define VC4CL_IMAGE

#include "Object.h"
#include "Context.h"

namespace vc4cl
{
	class Buffer;

	class Image
	{
	public:
		CHECK_RETURN cl_int getInfo(Buffer* buffer, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

		cl_channel_order channel_order;
		cl_channel_type channel_type;
		cl_mem_object_type image_type;
		size_t image_width;
		size_t image_height;
		size_t image_depth;
		size_t image_array_size;
		size_t image_row_pitch;
		size_t image_slice_pitch;
		cl_uint num_mip_levels;
		cl_uint num_samples;

	private:
		size_t calculateElementSize() const;
	};

	class Sampler : public Object<_cl_sampler, CL_INVALID_SAMPLER>, public HasContext
	{
	public:
		Sampler(Context* context, cl_bool normalizeCoords, cl_addressing_mode addressingMode, cl_filter_mode filterMode);
		~Sampler();

		CHECK_RETURN cl_int getInfo(cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

	private:

		cl_bool normalized_coords;
		cl_addressing_mode addressing_mode;
		cl_filter_mode filter_mode;
	};

} /* namespace vc4cl */

#endif /* VC4CL_IMAGE */
