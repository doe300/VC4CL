/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_IMAGE
#define VC4CL_IMAGE

#include <limits>

#include "Object.h"
#include "Context.h"
#include "Buffer.h"
#include "TextureFormat.h"

namespace vc4cl
{
	struct ChannelOrder
	{
		cl_channel_order id;
		unsigned char numChannels;

		constexpr ChannelOrder(cl_channel_order id, unsigned char channels) : id(id), numChannels(channels) { }

		static ChannelOrder fromID(cl_channel_order id);
	};

	static constexpr ChannelOrder CHANNEL_RED(CL_R, 1);
	static constexpr ChannelOrder CHANNEL_REDx(CL_Rx, 1);
	static constexpr ChannelOrder CHANNEL_ALPHA(CL_A, 1);
	static constexpr ChannelOrder CHANNEL_INTENSITY(CL_INTENSITY, 1);
	static constexpr ChannelOrder CHANNEL_LUMINANCE(CL_LUMINANCE, 1);
	static constexpr ChannelOrder CHANNEL_RED_GREEN(CL_RG, 2);
	static constexpr ChannelOrder CHANNEL_RED_GREENx(CL_RGx, 2);
	static constexpr ChannelOrder CHANNEL_RED_ALPHA(CL_RA, 2);
	static constexpr ChannelOrder CHANNEL_RED_GREEN_BLUE(CL_RGB, 3);
	static constexpr ChannelOrder CHANNEL_RED_GREEN_BLUEx(CL_RGBx, 3);
	static constexpr ChannelOrder CHANNEL_RED_GREEN_BLUE_ALPHA(CL_RGBA, 4);
	static constexpr ChannelOrder CHANNEL_ALPHA_RED_GREEN_BLUE(CL_ARGB, 4);
	static constexpr ChannelOrder CHANNEL_BLUE_GREEN_RED_ALPHA(CL_BGRA, 4);

	struct ChannelType
	{
		cl_channel_type id;
		unsigned char bytesPerComponent;
		bool isNormalized;
		bool isSigned;

		constexpr ChannelType(cl_channel_type id, unsigned char typeWidth, bool normalized, bool sign) : id(id), bytesPerComponent(typeWidth), isNormalized(normalized), isSigned(sign) { }

		template<typename T>
		constexpr static ChannelType create(cl_channel_type id, bool normalized = false)
		{
			//this wrapper is required, since C++ doesn't support specifying the type of template constructors
			return ChannelType(id, sizeof(T), normalized, std::numeric_limits<T>::is_signed);
		}

		static ChannelType fromID(cl_channel_type id);
	};

	static constexpr ChannelType CHANNEL_SNORM_INT8(ChannelType::create<signed char>(CL_SNORM_INT8, true));
	static constexpr ChannelType CHANNEL_SNORM_INT16(ChannelType::create<signed short>(CL_SNORM_INT16, true));
	static constexpr ChannelType CHANNEL_UNORM_INT8(ChannelType::create<unsigned char>(CL_UNORM_INT8, true));
	static constexpr ChannelType CHANNEL_UNORM_INT16(ChannelType::create<unsigned short>(CL_UNORM_INT16, true));
	static constexpr ChannelType CHANNEL_UNORM_SHORT_565(ChannelType::create<unsigned short>(CL_UNORM_SHORT_565, true));
	static constexpr ChannelType CHANNEL_UNORM_SHORT_555(ChannelType::create<unsigned short>(CL_UNORM_SHORT_555, true));
	static constexpr ChannelType CHANNEL_UNORM_INT_101010(ChannelType::create<unsigned int>(CL_UNORM_INT_101010, true));
	static constexpr ChannelType CHANNEL_SIGNED_INT8(ChannelType::create<signed char>(CL_SIGNED_INT8));
	static constexpr ChannelType CHANNEL_SIGNED_INT16(ChannelType::create<signed short>(CL_SIGNED_INT16));
	static constexpr ChannelType CHANNEL_SIGNED_INT32(ChannelType::create<signed int>(CL_SIGNED_INT32));
	static constexpr ChannelType CHANNEL_UNSIGNED_INT8(ChannelType::create<unsigned char>(CL_UNSIGNED_INT8));
	static constexpr ChannelType CHANNEL_UNSIGNED_INT16(ChannelType::create<unsigned short>(CL_UNSIGNED_INT16));
	static constexpr ChannelType CHANNEL_UNSIGNED_INT32(ChannelType::create<unsigned int>(CL_UNSIGNED_INT32));
	static constexpr ChannelType CHANNEL_HALF_FLOAT(CL_HALF_FLOAT, sizeof(short), false, true);
	static constexpr ChannelType CHANNEL_FLOAT(ChannelType::create<float>(CL_FLOAT));

	struct ImageType
	{
		cl_mem_object_type id;
		unsigned char numDimensions;
		bool isImageBuffer;
		bool isImageArray;

		constexpr ImageType(cl_mem_object_type id, unsigned char dims, bool isBuffer = false, bool isArray = false) : id(id), numDimensions(dims), isImageBuffer(isBuffer), isImageArray(isArray) { }

		static ImageType fromID(cl_mem_object_type id);
	};

	static constexpr ImageType IMAGE_1D(CL_MEM_OBJECT_IMAGE1D, 1);
	static constexpr ImageType IMAGE_1D_BUFFER(CL_MEM_OBJECT_IMAGE1D_BUFFER, 1, true);
	static constexpr ImageType IMAGE_1D_ARRAY(CL_MEM_OBJECT_IMAGE1D_ARRAY, 1, false, true);
	static constexpr ImageType IMAGE_2D(CL_MEM_OBJECT_IMAGE2D, 2);
	static constexpr ImageType IMAGE_2D_ARRAY(CL_MEM_OBJECT_IMAGE2D_ARRAY, 2, false, true);
	static constexpr ImageType IMAGE_3D(CL_MEM_OBJECT_IMAGE3D, 3);

	class Image : public Buffer
	{
	public:
		CHECK_RETURN cl_int getImageInfo(cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

		CHECK_RETURN cl_int getInfo(cl_mem_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret) override;

		uint32_t toBasicSetupValue() const;
		uint32_t toAccessSetupValue() const;
		uint32_t toExtendedSetupValue() const;
		size_t calculateElementSize() const;

		ChannelOrder channelOrder;
		ChannelType channelType;
		ImageType imageType;
		size_t imageWidth;
		size_t imageHeight;
		size_t imageDepth;
		size_t imageArraySize;
		size_t imageRowPitch;
		size_t imageSlicePitch;
		cl_uint numMipLevels;
		cl_uint numSamples;

	private:
		std::unique_ptr<TextureAccessor> accessor;
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
