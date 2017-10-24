/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Image.h"
#include "Buffer.h"

using namespace vc4cl;

ChannelOrder ChannelOrder::fromID(cl_channel_order id)
{
	switch(id)
	{
		case CL_R:
			return CHANNEL_RED;
		case CL_Rx:
			return CHANNEL_REDx;
		case CL_A:
			return CHANNEL_ALPHA;
		case CL_INTENSITY:
			return CHANNEL_INTENSITY;
		case CL_LUMINANCE:
			return CHANNEL_LUMINANCE;
		case CL_RG:
			return CHANNEL_RED_GREEN;
		case CL_RGx:
			return CHANNEL_RED_GREENx;
		case CL_RA:
			return CHANNEL_RED_ALPHA;
		case CL_RGB:
			return CHANNEL_RED_GREEN_BLUE;
		case CL_RGBx:
			return CHANNEL_RED_GREEN_BLUEx;
		case CL_RGBA:
			return CHANNEL_RED_GREEN_BLUE_ALPHA;
		case CL_BGRA:
			return CHANNEL_BLUE_GREEN_RED_ALPHA;
		case CL_ARGB:
			return CHANNEL_ALPHA_RED_GREEN_BLUE;
	}
	throw std::invalid_argument(std::string("Invalid channel-order: ") + std::to_string(id));
}

ChannelType ChannelType::fromID(cl_channel_type id)
{
	switch(id)
	{
		case CL_SNORM_INT8:
			return CHANNEL_SNORM_INT8;
		case CL_SNORM_INT16:
			return CHANNEL_SNORM_INT16;
		case CL_UNORM_INT8:
			return CHANNEL_UNORM_INT8;
		case CL_UNORM_INT16:
			return CHANNEL_UNORM_INT16;
		case CL_UNORM_SHORT_565:
			return CHANNEL_UNORM_SHORT_565;
		case CL_UNORM_SHORT_555:
			return CHANNEL_UNORM_SHORT_555;
		case CL_UNORM_INT_101010:
			return CHANNEL_UNORM_INT_101010;
		case CL_SIGNED_INT8:
			return CHANNEL_SIGNED_INT8;
		case CL_SIGNED_INT16:
			return CHANNEL_SIGNED_INT16;
		case CL_SIGNED_INT32:
			return CHANNEL_SIGNED_INT32;
		case CL_UNSIGNED_INT8:
			return CHANNEL_UNSIGNED_INT8;
		case CL_UNSIGNED_INT16:
			return CHANNEL_UNSIGNED_INT16;
		case CL_UNSIGNED_INT32:
			return CHANNEL_UNSIGNED_INT32;
		case CL_HALF_FLOAT:
			return CHANNEL_HALF_FLOAT;
		case CL_FLOAT:
			return CHANNEL_FLOAT;
	}
	throw std::invalid_argument(std::string("Invalid channel-type: ") + std::to_string(id));
}

ImageType ImageType::fromID(cl_mem_object_type id)
{
	switch(id)
	{
		case CL_MEM_OBJECT_IMAGE1D:
			return IMAGE_1D;
		case CL_MEM_OBJECT_IMAGE1D_BUFFER:
			return IMAGE_1D_BUFFER;
		case CL_MEM_OBJECT_IMAGE1D_ARRAY:
			return IMAGE_1D_ARRAY;
		case CL_MEM_OBJECT_IMAGE2D:
			return IMAGE_2D;
		case CL_MEM_OBJECT_IMAGE2D_ARRAY:
			return IMAGE_2D_ARRAY;
		case CL_MEM_OBJECT_IMAGE3D:
			return IMAGE_3D;
	}
	throw std::invalid_argument(std::string("Invalid image-type: ") + std::to_string(id));
}

cl_int Image::getImageInfo(cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	cl_image_format format;
	format.image_channel_data_type = channelType.id;
	format.image_channel_order = channelOrder.id;

	switch(param_name)
	{
		case CL_IMAGE_FORMAT:
			return returnValue(&format, sizeof(cl_image_format), 1, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_ELEMENT_SIZE:
			//"Return size of each element of the image memory object given by image.
			// An element is made up of n channels. The value of n is given in cl_image_format descriptor."
			return returnValue<size_t>(calculateElementSize(), param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_ROW_PITCH:
			return returnValue<size_t>(imageRowPitch, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_SLICE_PITCH:
			return returnValue<size_t>(imageSlicePitch, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_WIDTH:
			return returnValue<size_t>(imageWidth, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_HEIGHT:
			return returnValue<size_t>(imageHeight, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_DEPTH:
			return returnValue<size_t>(imageDepth, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_ARRAY_SIZE:
			return returnValue<size_t>(imageArraySize, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_BUFFER:
			return returnValue<cl_mem>(toBase(), param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_NUM_MIP_LEVELS:
			return returnValue<cl_uint>(numMipLevels, param_value_size, param_value, param_value_size_ret);
		case CL_IMAGE_NUM_SAMPLES:
			return returnValue<cl_uint>(numSamples, param_value_size, param_value, param_value_size_ret);
	}

	return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_image_info value %d", param_name));
}

cl_int Image::getInfo(cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	if(param_name == CL_MEM_TYPE)
		return returnValue<cl_mem_object_type>(imageType.id, param_value_size, param_value, param_value_size_ret);
	return Buffer::getInfo(param_name, param_value_size, param_value_size_ret, param_value_size_ret);
}

size_t Image::calculateElementSize() const
{
	return channelType.bytesPerComponent * channelOrder.numChannels;
}

Sampler::Sampler(Context* context, cl_bool normalizeCoords, cl_addressing_mode addressingMode, cl_filter_mode filterMode) : HasContext(context), normalized_coords(normalizeCoords),
		addressing_mode(addressingMode), filter_mode(filterMode)
{
}

Sampler::~Sampler()
{
}

cl_int Sampler::getInfo(cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	switch(param_name)
	{
		case CL_SAMPLER_REFERENCE_COUNT:
			return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
		case CL_SAMPLER_CONTEXT:
			return returnValue<cl_context>(context()->toBase(), param_value_size, param_value, param_value_size_ret);
		case CL_SAMPLER_NORMALIZED_COORDS:
			return returnValue<cl_bool>(normalized_coords, param_value_size, param_value, param_value_size_ret);
		case CL_SAMPLER_ADDRESSING_MODE:
			return returnValue<cl_addressing_mode>(addressing_mode, param_value_size, param_value, param_value_size_ret);
		case CL_SAMPLER_FILTER_MODE:
			return returnValue<cl_filter_mode>(filter_mode, param_value_size, param_value, param_value_size_ret);
	}

	return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_sampler_info value %d", param_name));
}

static cl_bool check_image_format(const cl_image_format* format)
{
	if(format == NULL)
		return CL_FALSE;

	/*
	 * EMBEDDED_PROFILE requires following data-types to be supported (OpenCL 1.2, page 357):
	 * CL_UNORM_INT8, CL_UNORM_INT16,
	 * CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32,
	 * CL_UNSIGNED_INT8, CL_UNSIGNED_INT16, CL_UNSIGNED_INT32,
	 * CL_HALF_FLOAT, CL_FLOAT
	 */
	if(format->image_channel_data_type != CL_UNORM_INT8 && format->image_channel_data_type != CL_UNORM_INT16 &&
			format->image_channel_data_type != CL_SIGNED_INT8 && format->image_channel_data_type != CL_SIGNED_INT16 &&
			format->image_channel_data_type != CL_SIGNED_INT32 && format->image_channel_data_type != CL_UNSIGNED_INT8 &&
			format->image_channel_data_type != CL_UNSIGNED_INT16 && format->image_channel_data_type != CL_UNSIGNED_INT32 &&
			format->image_channel_data_type != CL_HALF_FLOAT && format->image_channel_data_type != CL_FLOAT)
		//for simplicity, don't support any other format than required
		return CL_FALSE;

	//EMBEDDED_PROFILE requires only CL_RGBA to be supported (OpenCL 1.2, page 357)
	//so we simplify things by disallowing most other formats
	if(format->image_channel_order != CL_RGBA && format->image_channel_order != CL_RGB && format->image_channel_order != CL_INTENSITY && format->image_channel_order != CL_LUMINANCE)
		return CL_FALSE;

	//TODO instead of this, check against the list of supported formats below

	return CL_TRUE;
}

static size_t calculate_image_size(const cl_image_desc* description)
{
	size_t image_size = 0;
	//see OpenCL 1.2 standard page 92
	switch(description->image_type)
	{
		case CL_MEM_OBJECT_IMAGE1D:
		case CL_MEM_OBJECT_IMAGE1D_BUFFER:
			image_size = description->image_row_pitch;
			break;
		case CL_MEM_OBJECT_IMAGE2D:
			image_size = description->image_row_pitch * description->image_height;
			break;
		case CL_MEM_OBJECT_IMAGE3D:
			image_size = description->image_slice_pitch * description->image_depth;
			break;
		case CL_MEM_OBJECT_IMAGE1D_ARRAY:
		case CL_MEM_OBJECT_IMAGE2D_ARRAY:
			image_size = description->image_slice_pitch * description->image_array_size;
			break;
	}
	return image_size;
}

/*
 * OpenCL 1.2 specification, pages 91+:
 *
 *  A 1D image, 1D image buffer, 1D image array, 2D image, 2D image array and 3D image object can be created using the following function:
 *
 *  \param context is a valid OpenCL context on which the image object is to be created.
 *
 *  \param flags is a bit-field that is used to specify allocation and usage information about the image memory object being created and is described in table 5.3.
 *  For all image types except CL_MEM_OBJECT_IMAGE1D_BUFFER , if value specified for flags is 0, the default is used which is CL_MEM_READ_WRITE .
 *  For CL_MEM_OBJECT_IMAGE1D_BUFFER image type, if the CL_MEM_READ_WRITE , CL_MEM_READ_ONLY or CL_MEM_WRITE_ONLY values are not specified in flags,
 *  they are inherited from the corresponding memory access qualifers associated with buffer. The CL_MEM_USE_HOST_PTR ,
 *  CL_MEM_ALLOC_HOST_PTR and CL_MEM_COPY_HOST_PTR values cannot be specified in flags but are inherited from the corresponding memory access qualifiers associated with buffer.
 *  If CL_MEM_COPY_HOST_PTR is specified in the memory access qualifier values associated with buffer it does not imply any additional copies when the sub-buffer is created from buffer.
 *  If the CL_MEM_HOST_WRITE_ONLY , CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS values are not specified in flags, they are inherited from the corresponding memory access qualifiers associated with buffer.
 *
 *  \param image_format is a pointer to a structure that describes format properties of the image to be allocated.
 *  Refer to section 5.3.1.1 for a detailed description of the image format descriptor.
 *
 *  \param image_desc is a pointer to a structure that describes type and dimensions of the image to be allocated.
 *  Refer to section 5.3.1.2 for a detailed description of the image descriptor.
 *
 *  \param host_ptr is a pointer to the image data that may already be allocated by the application.
 *  Refer to table below for a description of how large the buffer that host_ptr points to must be.
 *  For a 3D image or 2D image array, the image data specified by host_ptr is stored as a linear sequence of adjacent 2D image slices or 2D images respectively.
 *  Each 2D image is a linear sequence of adjacent scanlines. Each scanline is a linear sequence of image elements.
 *  For a 2D image, the image data specified by host_ptr is stored as a linear sequence of adjacent scanlines.
 *  Each scanline is a linear sequence of image elements. For a 1D image array, the image data specified by host_ptr is stored as a linear sequence of adjacent 1D images respectively.
 *  Each 1D image or 1D image buffer is a single scanline which is a linear sequence of adjacent elements.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clCreateImage returns a valid non-zero image object created and the errcode_ret is set to CL_SUCCESS if the image object is created successfully.
 *  Otherwise, it returns a NULL value with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if values specified in flags are not valid.
 *  - CL_INVALID_IMAGE_FORMAT_DESCRIPTOR if values specified in image_format are not valid or if image_format is NULL .
 *  - CL_INVALID_IMAGE_DESCRIPTOR if values specified in image_desc are not valid or if image_desc is NULL .
 *  - CL_INVALID_IMAGE_SIZE if image dimensions specified in image_desc exceed the minimum maximum image dimensions described in table 4.3 for all devices in context.
 *  - CL_INVALID_HOST_PTR if host_ptr is NULL and CL_MEM_USE_HOST_PTR or CL_MEM_COPY_HOST_PTR are set in flags or if host_ptr is not NULL but CL_MEM_COPY_HOST_PTR or CL_MEM_USE_HOST_PTR are not set in flags.
 *  - CL_INVALID_VALUE if a 1D image buffer is being created and the buffer object was created with CL_MEM_WRITE_ONLY and flags specifies CL_MEM_READ_WRITE or CL_MEM_READ_ONLY ,
 *  or if the buffer object was created with CL_MEM_READ_ONLY and flags specifies CL_MEM_READ_WRITE or CL_MEM_WRITE_ONLY , or if flags specifies CL_MEM_USE_HOST_PTR or CL_MEM_ALLOC_HOST_PTR or CL_MEM_COPY_HOST_PTR .
 *  - CL_INVALID_VALUE if a 1D image buffer is being created and the buffer object was created with CL_MEM_HOST_WRITE_ONLY and flags specifies CL_MEM_HOST_READ_ONLY ,
 *  or if the buffer object was created with CL_MEM_HOST_READ_ONLY and flags specifies CL_MEM_HOST_WRITE_ONLY , or if the buffer object was created with CL_MEM_HOST_NO_ACCESS
 *  and flags specifies CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_WRITE_ONLY .
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if the image_format is not supported.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for image object.
 *  - CL_INVALID_OPERATION if there are no devices in context that support images (i.e. CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_mem VC4CL_FUNC(clCreateImage)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format, const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_mem)
#ifndef IMAGE_SUPPORT
	return returnError<cl_mem>(CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Image support is not enabled!");
#endif

	if(image_format == NULL)
		return returnError<cl_mem>(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, errcode_ret, __FILE__, __LINE__, "Image format is not set!");
	if(image_desc == NULL)
		return returnError<cl_mem>(CL_INVALID_IMAGE_DESCRIPTOR, errcode_ret, __FILE__, __LINE__, "Image description is not set!");
	if(check_image_format(image_format) == CL_FALSE)
		return returnError<cl_mem>(CL_IMAGE_FORMAT_NOT_SUPPORTED, errcode_ret, __FILE__, __LINE__, buildString("Unsupported image format (type: %u, order: %u)!", image_format->image_channel_data_type, image_format->image_channel_order));

	if(image_desc->image_width > kernel_config::MAX_IMAGE_DIMENSION)
		return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__, buildString("Image width (%u) exceeds supported maximum (%u)!", image_desc->image_width, kernel_config::MAX_IMAGE_DIMENSION));
	if((image_desc->image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY || image_desc->image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) && image_desc->image_array_size > kernel_config::MAX_IMAGE_DIMENSION)
		return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__, buildString("Image array size (%u) exceeds supported maximum (%u)!", image_desc->image_array_size, kernel_config::MAX_IMAGE_DIMENSION));
	if((image_desc->image_type == CL_MEM_OBJECT_IMAGE2D || image_desc->image_type == CL_MEM_OBJECT_IMAGE3D || image_desc->image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) && image_desc->image_height > kernel_config::MAX_IMAGE_DIMENSION)
		return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__, buildString("Image height (%u) exceeds supported maximum (%u)!", image_desc->image_height, kernel_config::MAX_IMAGE_DIMENSION));
	if(image_desc->image_type == CL_MEM_OBJECT_IMAGE3D && image_desc->image_depth > kernel_config::MAX_IMAGE_DIMENSION)
		return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__, buildString("Image depth (%u) exceeds supported maximum (%u)!", image_desc->image_depth, kernel_config::MAX_IMAGE_DIMENSION));

	//calculate buffer size
	size_t size = calculate_image_size(image_desc);
	if(size == 0)
		return returnError<cl_mem>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Image has no size!");

	//add image-info overhead to size
	size += sizeof(Image);

	//create buffer
	//TODO create image object, need to run all the checks in here!
	cl_mem buffer = VC4CL_FUNC(clCreateBuffer)(context, flags, size, host_ptr, errcode_ret);
	if(buffer == NULL)
		return returnError<cl_mem>(*errcode_ret, errcode_ret, __FILE__, __LINE__, "Failed to create buffer for image!");

	Image* img = toType<Image>(buffer);
	img->channelOrder = ChannelOrder::fromID(image_format->image_channel_order);
	img->channelType = ChannelType::fromID(image_format->image_channel_data_type);
	img->imageArraySize = image_desc->image_array_size;
	img->imageDepth = image_desc->image_depth;
	img->imageHeight = image_desc->image_height;
	img->imageRowPitch = image_desc->image_row_pitch;
	img->imageSlicePitch = image_desc->image_slice_pitch;
	img->imageType = ImageType::fromID(image_desc->image_type);
	img->imageWidth = image_desc->image_width;
	img->numMipLevels = image_desc->num_mip_levels;
	img->numSamples = image_desc->num_samples;

	//TODO if pitches are not set (0), calculate to fit padding of texture format

	return buffer;
}

cl_mem VC4CL_FUNC(clCreateImage2D)(cl_context context, cl_mem_flags flags, const cl_image_format *image_format, size_t image_width, size_t image_height, size_t image_row_pitch, void *host_ptr, cl_int *errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_mem)

	cl_image_desc desc;
	desc.image_type = CL_MEM_OBJECT_IMAGE2D;
	desc.image_width = image_width;
	desc.image_height = image_height;
	desc.image_depth = 1;
	desc.image_array_size = 1;
	desc.image_row_pitch = image_row_pitch;
	desc.image_slice_pitch = 0;
	desc.num_mip_levels = 0;
	desc.num_samples = 0;
	desc.buffer = NULL;
	return VC4CL_FUNC(clCreateImage)(context, flags, image_format, &desc, host_ptr, errcode_ret);
}

cl_mem VC4CL_FUNC(clCreateImage3D)(cl_context context, cl_mem_flags flags, const cl_image_format *image_format, size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch, void *host_ptr, cl_int *errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_mem)

	cl_image_desc desc;
	desc.image_type = CL_MEM_OBJECT_IMAGE3D;
	desc.image_width = image_width;
	desc.image_height = image_height;
	desc.image_depth = image_depth;
	desc.image_array_size = 1;
	desc.image_row_pitch = image_row_pitch;
	desc.image_slice_pitch = image_slice_pitch;
	desc.num_mip_levels = 0;
	desc.num_samples = 0;
	desc.buffer = NULL;
	return VC4CL_FUNC(clCreateImage)(context, flags, image_format, &desc, host_ptr, errcode_ret);
}

static const cl_image_format supported_formats[] = {
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_SNORM_INT8},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_SNORM_INT16},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_UNORM_INT8},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_UNORM_INT16},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_SIGNED_INT8},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_SIGNED_INT16},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_SIGNED_INT32},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_UNSIGNED_INT8},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_UNSIGNED_INT16},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_UNSIGNED_INT32},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_HALF_FLOAT},
	{.image_channel_order = CL_RGBA, .image_channel_data_type = CL_FLOAT},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_SNORM_INT8},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_SNORM_INT16},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_UNORM_INT8},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_UNORM_INT16},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_SIGNED_INT8},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_SIGNED_INT16},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_SIGNED_INT32},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_UNSIGNED_INT8},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_UNSIGNED_INT16},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_UNSIGNED_INT32},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_HALF_FLOAT},
	{.image_channel_order = CL_RGB, .image_channel_data_type = CL_FLOAT},
	//XXX CL_ILLUMINANCE, CL_INTENSITY??
	//TODO overhaul with really supported formats!
};

/*
 * OpenCL 1.2 specification, pages 97+:
 *
 *  Can be used to get the list of image formats supported by an OpenCL implementation when the following information about an image memory object is specified:
 *  - Context
 *  - Image type – 1D, 2D, or 3D image, 1D image buffer, 1D or 2D image array.
 *  - Image object allocation information
 *
 *  clGetSupportedImageFormats returns a union of image formats supported by all devices in the context.
 *
 *  \param context is a valid OpenCL context on which the image object(s) will be created.
 *
 *  \param flags is a bit-field that is used to specify allocation and usage information about the image memory object being created and is described in table 5.3.
 *
 *  \param image_type describes the image type and must be either CL_MEM_OBJECT_IMAGE1D ,CL_MEM_OBJECT_IMAGE1D_BUFFER , CL_MEM_OBJECT_IMAGE2D ,CL_MEM_OBJECT_IMAGE3D , CL_MEM_OBJECT_IMAGE1D_ARRAY or
 *  CL_MEM_OBJECT_IMAGE2D_ARRAY .
 *
 *  \param num_entries specifies the number of entries that can be returned in the memory location given by image_formats.
 *
 *  \param image_formats is a pointer to a memory location where the list of supported image formats are returned. Each entry describes a cl_image_format structure supported by the OpenCL implementation.
 *  If image_formats is NULL , it is ignored.
 *
 *  \param num_image_formats is the actual number of supported image formats for a specific context and values specified by flags. If num_image_formats is NULL , it is ignored.
 *
 *  \return clGetSupportedImageFormats returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if flags or image_type are not valid, or if num_entries is 0 and image_formats is not NULL.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 *
 *
 *  If CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_TRUE , the values assigned to CL_DEVICE_MAX_READ_IMAGE_ARGS , CL_DEVICE_MAX_WRITE_IMAGE_ARGS ,CL_DEVICE_IMAGE2D_MAX_WIDTH , CL_DEVICE_IMAGE2D_MAX_HEIGHT ,
 *  CL_DEVICE_IMAGE3D_MAX_WIDTH , CL_DEVICE_IMAGE3D_MAX_HEIGHT , CL_DEVICE_IMAGE3D_MAX_DEPTH and CL_DEVICE_MAX_SAMPLERS by the implementation must be greater than or equal to the minimum values specified in table 4.3.
 */
cl_int VC4CL_FUNC(clGetSupportedImageFormats)(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type, cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats)
{
	CHECK_CONTEXT(toType<Context>(context))
#ifndef IMAGE_SUPPORT
	return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Image support is not enabled!");
#endif

	if((num_entries == 0) != (image_formats == NULL))
		return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameters are empty!");

	const cl_uint num_formats = sizeof(supported_formats) / sizeof(cl_image_format);

	if(num_entries >= num_formats)
		memcpy(image_formats, supported_formats, sizeof(supported_formats));
	if(num_image_formats != NULL)
		*num_image_formats = num_formats;

	return CL_SUCCESS;
}

cl_int VC4CL_FUNC(clEnqueueReadImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read, const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_BUFFER(toType<Buffer>(image))

			//TODO
	return CL_INVALID_OPERATION;
}

cl_int VC4CL_FUNC(clEnqueueWriteImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write, const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_BUFFER(toType<Buffer>(image))

	//TODO
	return CL_INVALID_OPERATION;
}

cl_int VC4CL_FUNC(clEnqueueCopyImage)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image, const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_BUFFER(toType<Buffer>(src_image))
	CHECK_BUFFER(toType<Buffer>(dst_image))

			//TODO
	return CL_INVALID_OPERATION;
}

cl_int VC4CL_FUNC(clEnqueueFillImage)(cl_command_queue command_queue, cl_mem image, const void* fill_color, const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_BUFFER(toType<Buffer>(image))

			//TODO
	return CL_INVALID_OPERATION;
}

cl_int VC4CL_FUNC(clEnqueueCopyImageToBuffer)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer, const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_BUFFER(toType<Buffer>(src_image))
	CHECK_BUFFER(toType<Buffer>(dst_buffer))

			//TODO
	return CL_INVALID_OPERATION;
}

cl_int VC4CL_FUNC(clEnqueueCopyBufferToImage)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image, size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
	CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
	CHECK_BUFFER(toType<Buffer>(src_buffer))
	CHECK_BUFFER(toType<Buffer>(dst_image))

			//TODO
	return CL_INVALID_OPERATION;
}

void* VC4CL_FUNC(clEnqueueMapImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_map, cl_map_flags map_flags, const size_t* origin, const size_t* region, size_t* image_row_pitch, size_t* image_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event, cl_int* errcode_ret)
{
	CHECK_COMMAND_QUEUE_ERROR_CODE(toType<CommandQueue>(command_queue), errcode_ret, void*)
	CHECK_BUFFER_ERROR_CODE(toType<Buffer>(image), errcode_ret, void*)

			//TODO
	return returnError<void*>(CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Not supported!");
}

cl_int VC4CL_FUNC(clGetImageInfo)(cl_mem image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	CHECK_BUFFER(toType<Image>(image))
	return toType<Image>(image)->getImageInfo(param_name, param_value_size, param_value, param_value_size_ret);
}

/*!
 * OpenCL 1.2 specification, page 129+:
 *
 *  Creates a sampler object. Refer to section 6.12.14.1 for a detailed description of how samplers work.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param normalized_coords determines if the image coordinates specified are normalized (if normalized_coords is CL_TRUE ) or not (if normalized_coords is CL_FALSE ).
 *
 *  \param addressing_mode specifies how out-of-range image coordinates are handled when reading from an image. This can be set to CL_ADDRESS_MIRRORED_REPEAT , CL_ADDRESS_REPEAT ,
 *   CL_ADDRESS_CLAMP_TO_EDGE , CL_ADDRESS_CLAMP and CL_ADDRESS_NONE .
 *
 *  \param filter_mode specifies the type of filter that must be applied when reading an image. This can be CL_FILTER_NEAREST , or CL_FILTER_LINEAR .
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clCreateSampler returns a valid non-zero sampler object and errcode_ret is set to CL_SUCCESS if the sampler object is created successfully.
 *  Otherwise, it returns a NULL value with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if addressing_mode, filter_mode or normalized_coords or combination of these argument values are not valid.
 *  - CL_INVALID_OPERATION if images are not supported by any device associated with context (i.e. CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_sampler VC4CL_FUNC(clCreateSampler)(cl_context context, cl_bool normalized_coords, cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret)
{
	CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_sampler)
#ifndef IMAGE_SUPPORT
	return returnError<cl_sampler>(CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Image support is not enabled!");
#endif

	if(normalized_coords != CL_TRUE && normalized_coords != CL_FALSE)
		return returnError<cl_sampler>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Invalid value for normalized coordinates!");

	//see OpenCL 1.2 specification, table 6.22
	if(normalized_coords != CL_TRUE && addressing_mode == CL_ADDRESS_MIRRORED_REPEAT)
		return returnError<cl_sampler>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Mirrored repeat addressing mode can only be used with normalized coordinates!");
	if(normalized_coords != CL_TRUE && addressing_mode == CL_ADDRESS_REPEAT)
		return returnError<cl_sampler>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Repeat addressing mode can only be used with normalized coordinates!");

	Sampler* sampler = newObject<Sampler>(toType<Context>(context), normalized_coords, addressing_mode, filter_mode);
	CHECK_ALLOCATION_ERROR_CODE(sampler, errcode_ret, cl_sampler)
	RETURN_OBJECT(sampler->toBase(), errcode_ret)
}

/*!
 * OpenCL 1.2 specification, page 130:
 *
 *  Increments the sampler reference count. clCreateSampler performs an implicit retain.
 *
 *  \return clRetainSampler returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_SAMPLER if sampler is not a valid sampler object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clRetainSampler)(cl_sampler sampler)
{
	CHECK_SAMPLER(toType<Sampler>(sampler))
	return toType<Sampler>(sampler)->retain();
}

/*!
 * OpenCL 1.2 specification, page 130:
 *
 *  Decrements the sampler reference count. The sampler object is deleted after the reference count becomes zero and commands queued for execution on a command-queue(s) that use sampler have finished.
 *
 *  \return clReleaseSampler returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_SAMPLER if sampler is not a valid sampler object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clReleaseSampler)(cl_sampler sampler)
{
	CHECK_SAMPLER(toType<Sampler>(sampler))
	return toType<Sampler>(sampler)->release();
}

/*!
 * OpenCL 1.2 specification, pages 130+:
 *
 *  Returns information about the sampler object.
 *
 *  \param sampler specifies the sampler being queried.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information returned in param_value by clGetSamplerInfo is described in table 5.12.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be >= size of return type as described in table 5.12.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret is NULL , it is ignored.
 *
 *  \return clGetSamplerInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return type as described in table 5.12 and param_value is not NULL .
 *  - CL_INVALID_SAMPLER if sampler is a not a valid sampler object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the host.
 */
cl_int VC4CL_FUNC(clGetSamplerInfo)(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
	CHECK_SAMPLER(toType<Sampler>(sampler))
	return toType<Sampler>(sampler)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
