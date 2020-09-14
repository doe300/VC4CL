/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_IMAGE
#define VC4CL_IMAGE

#include "Buffer.h"
#include "Context.h"
#include "TextureConfiguration.h"
#include "TextureFormat.h"

#include <limits>

namespace vc4cl
{
    struct ChannelOrder
    {
        cl_channel_order id;
        unsigned char numChannels;

        constexpr ChannelOrder(cl_channel_order id, unsigned char channels) noexcept : id(id), numChannels(channels) {}
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
    static constexpr ChannelOrder CHANNEL_Y_U_Y_V(CL_YUYV_INTEL, 4);

    struct ChannelType
    {
        cl_channel_type id;
        unsigned char bytesPerComponent;
        bool isNormalized;
        bool isSigned;

        constexpr ChannelType(cl_channel_type id, unsigned char typeWidth, bool normalized, bool sign) noexcept :
            id(id), bytesPerComponent(typeWidth), isNormalized(normalized), isSigned(sign)
        {
        }

        template <typename T>
        constexpr static ChannelType create(cl_channel_type id, bool normalized = false) noexcept
        {
            // this wrapper is required, since C++ doesn't support specifying the type of template constructors
            return ChannelType(id, sizeof(T), normalized, std::numeric_limits<T>::is_signed);
        }
    };

    static constexpr ChannelType CHANNEL_SNORM_INT8(ChannelType::create<int8_t>(CL_SNORM_INT8, true));
    static constexpr ChannelType CHANNEL_SNORM_INT16(ChannelType::create<int16_t>(CL_SNORM_INT16, true));
    static constexpr ChannelType CHANNEL_UNORM_INT8(ChannelType::create<uint8_t>(CL_UNORM_INT8, true));
    static constexpr ChannelType CHANNEL_UNORM_INT16(ChannelType::create<uint16_t>(CL_UNORM_INT16, true));
    static constexpr ChannelType CHANNEL_UNORM_SHORT_565(ChannelType::create<uint16_t>(CL_UNORM_SHORT_565, true));
    static constexpr ChannelType CHANNEL_UNORM_SHORT_555(ChannelType::create<uint16_t>(CL_UNORM_SHORT_555, true));
    static constexpr ChannelType CHANNEL_UNORM_INT_101010(ChannelType::create<uint32_t>(CL_UNORM_INT_101010, true));
    static constexpr ChannelType CHANNEL_SIGNED_INT8(ChannelType::create<int8_t>(CL_SIGNED_INT8));
    static constexpr ChannelType CHANNEL_SIGNED_INT16(ChannelType::create<int16_t>(CL_SIGNED_INT16));
    static constexpr ChannelType CHANNEL_SIGNED_INT32(ChannelType::create<int32_t>(CL_SIGNED_INT32));
    static constexpr ChannelType CHANNEL_UNSIGNED_INT8(ChannelType::create<uint8_t>(CL_UNSIGNED_INT8));
    static constexpr ChannelType CHANNEL_UNSIGNED_INT16(ChannelType::create<uint16_t>(CL_UNSIGNED_INT16));
    static constexpr ChannelType CHANNEL_UNSIGNED_INT32(ChannelType::create<uint32_t>(CL_UNSIGNED_INT32));
    static constexpr ChannelType CHANNEL_HALF_FLOAT(CL_HALF_FLOAT, sizeof(short), false, true);
    static constexpr ChannelType CHANNEL_FLOAT(ChannelType::create<float>(CL_FLOAT));

    struct ImageType
    {
        cl_mem_object_type id;
        unsigned char numDimensions;
        bool isImageBuffer;
        bool isImageArray;

        constexpr ImageType(
            cl_mem_object_type id, unsigned char dims, bool isBuffer = false, bool isArray = false) noexcept :
            id(id),
            numDimensions(dims), isImageBuffer(isBuffer), isImageArray(isArray)
        {
        }
    };

    static constexpr ImageType IMAGE_1D(CL_MEM_OBJECT_IMAGE1D, 1);
    static constexpr ImageType IMAGE_1D_BUFFER(CL_MEM_OBJECT_IMAGE1D_BUFFER, 1, true);
    static constexpr ImageType IMAGE_1D_ARRAY(CL_MEM_OBJECT_IMAGE1D_ARRAY, 1, false, true);
    static constexpr ImageType IMAGE_2D(CL_MEM_OBJECT_IMAGE2D, 2);
    static constexpr ImageType IMAGE_2D_ARRAY(CL_MEM_OBJECT_IMAGE2D_ARRAY, 2, false, true);
    static constexpr ImageType IMAGE_3D(CL_MEM_OBJECT_IMAGE3D, 3);

    class Image final : public Buffer
    {
    public:
        Image(Context* context, cl_mem_flags flags, const cl_image_format& imageFormat,
            const cl_image_desc& imageDescription);

        CHECK_RETURN cl_int getImageInfo(
            cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);
        CHECK_RETURN cl_int getInfo(cl_mem_info param_name, size_t param_value_size, void* param_value,
            size_t* param_value_size_ret) override final;

        CHECK_RETURN cl_int enqueueRead(CommandQueue* commandQueue, cl_bool blockingRead, const size_t* origin,
            const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint numEventsInWaitList,
            const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueWrite(CommandQueue* commandQueue, cl_bool blockingWrite, const size_t* origin,
            const size_t* region, size_t row_pitch, size_t slice_pitch, const void* ptr, cl_uint numEventsInWaitList,
            const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueCopyInto(CommandQueue* commandQueue, Image* destination, const size_t* srcOrigin,
            const size_t* dstOrigin, const size_t* region, cl_uint numEventsInWaitList, const cl_event* waitList,
            cl_event* event);
        CHECK_RETURN cl_int enqueueFill(CommandQueue* commandQueue, const void* color, const size_t* origin,
            const size_t* region, cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event);
        CHECK_RETURN cl_int enqueueCopyFromToBuffer(CommandQueue* commandQueue, Buffer* buffer, const size_t* origin,
            const size_t* region, size_t bufferOffset, bool copyIntoImage, cl_uint numEventsInWaitList,
            const cl_event* waitList, cl_event* event);
        CHECK_RETURN void* enqueueMap(CommandQueue* commandQueue, cl_bool blockingMap, cl_map_flags mapFlags,
            const size_t* origin, const size_t* region, size_t* rowPitchOutput, size_t* slicePitchOutput,
            cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event, cl_int* errcode_ret);

        TextureConfiguration toTextureConfiguration() const;

        size_t calculateElementSize() const __attribute__((pure));

        ChannelOrder channelOrder;
        ChannelType channelType;
        TextureType textureType;
        ImageType imageType;
        size_t imageWidth;
        size_t imageHeight;
        size_t imageDepth;
        size_t imageArraySize;
        size_t imageRowPitch;
        size_t imageSlicePitch;
        cl_uint numMipLevels;
        cl_uint numSamples;
        std::unique_ptr<TextureAccessor> accessor;

    private:
        CHECK_RETURN cl_int checkImageAccess(const size_t* origin, const size_t* region) const;
        CHECK_RETURN cl_int checkImageSlices(const size_t* region, size_t row_pitch, size_t slice_pitch) const;
    };

    class Sampler final : public Object<_cl_sampler, CL_INVALID_SAMPLER>, public HasContext
    {
    public:
        Sampler(Context* context, bool normalizeCoords, cl_addressing_mode addressingMode, cl_filter_mode filterMode);
        ~Sampler() noexcept override;

        CHECK_RETURN cl_int getInfo(
            cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret);

    private:
        bool normalized_coords;
        cl_addressing_mode addressing_mode;
        cl_filter_mode filter_mode;
    };

    struct ImageAccess final : public EventAction
    {
        object_wrapper<Image> image;
        void* hostPointer;
        bool writeToImage;
        std::array<size_t, 3> origin;
        std::array<size_t, 3> region;
        size_t hostRowPitch;
        size_t hostSlicePitch;

        ImageAccess(
            Image* image, void* hostPtr, bool writeImage, const std::size_t origin[3], const std::size_t region[3]);
        ~ImageAccess() override;

        cl_int operator()() override final;
    };

    struct ImageCopy final : public EventAction
    {
        object_wrapper<Image> source;
        object_wrapper<Image> destination;
        std::array<size_t, 3> sourceOrigin;
        std::array<size_t, 3> destOrigin;
        std::array<size_t, 3> region;

        ImageCopy(Image* src, Image* dst, const std::size_t srcOrigin[3], const std::size_t dstOrigin[3],
            const std::size_t region[3]);
        ~ImageCopy() override;

        cl_int operator()() override final;
    };

    struct ImageFill final : public EventAction
    {
        object_wrapper<Image> image;
        std::array<size_t, 3> origin;
        std::array<size_t, 3> region;
        std::vector<char> fillColor;

        ImageFill(Image* img, const void* color, const std::size_t origin[3], const std::size_t region[3]);
        ~ImageFill() override;

        cl_int operator()() override final;
    };

    struct ImageCopyBuffer final : public EventAction
    {
        object_wrapper<Image> image;
        object_wrapper<Buffer> buffer;
        bool copyIntoImage;
        std::array<size_t, 3> imageOrigin;
        std::array<size_t, 3> imageRegion;
        std::size_t bufferOffset;

        ImageCopyBuffer(Image* image, Buffer* buffer, bool copyIntoImage, const std::size_t imgOrigin[3],
            const std::size_t region[3], size_t bufferOffset);
        ~ImageCopyBuffer() override;

        cl_int operator()() override final;
    };

    struct ImageMapping final : public BufferMapping
    {
        std::array<size_t, 3> origin;
        std::array<size_t, 3> region;

        ImageMapping(Image* image, std::list<MappingInfo>::const_iterator mappingInfo, bool isUnmap,
            const std::size_t origin[3], const std::size_t region[3]);
        ~ImageMapping() override;
    };

    struct hash_cl_image_format : public std::hash<std::string>
    {
        size_t operator()(const cl_image_format& format) const noexcept __attribute__((pure));
    };

    struct equal_cl_image_format : public std::equal_to<cl_image_format>
    {
        bool operator()(const cl_image_format& f1, const cl_image_format& f2) const noexcept __attribute__((pure));
    };

} /* namespace vc4cl */

#endif /* VC4CL_IMAGE */
