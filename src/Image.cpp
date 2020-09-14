/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "Image.h"

#include "Buffer.h"

#include <unordered_map>

using namespace vc4cl;

/*
 * EMBEDDED_PROFILE requires following data-types (for channel-type CL_RGBA) to be supported (OpenCL 1.2, page 357):
 *
 * "For 1D, 2D, optional 3D images,1D and 2D image array objects, the minimum list of supported image formats (for
 * reading and writing) is:" CL_UNORM_INT8, CL_UNORM_INT16, CL_SIGNED_INT8, CL_SIGNED_INT16, CL_SIGNED_INT32,
 * CL_UNSIGNED_INT8, CL_UNSIGNED_INT16, CL_UNSIGNED_INT32,
 * CL_HALF_FLOAT, CL_FLOAT
 *
 * XXX -> we don't have to support image1d_buffer?
 */
static const std::unordered_map<cl_image_format, TextureType, vc4cl::hash_cl_image_format, equal_cl_image_format>
    supportedFormats = {
        // Required by the OpenCL 1.2 specification
        {cl_image_format{CL_RGBA, CL_UNORM_INT8}, RGBA32R}, {cl_image_format{CL_RGBA, CL_UNORM_INT16}, S16},   // XXX
        {cl_image_format{CL_RGBA, CL_SIGNED_INT8}, RGBA32R}, {cl_image_format{CL_RGBA, CL_SIGNED_INT16}, S16}, // XXX
        {cl_image_format{CL_RGBA, CL_SIGNED_INT32}, A1},                                                       // XXX
        {cl_image_format{CL_RGBA, CL_UNSIGNED_INT8}, RGBA32R},
        {cl_image_format{CL_RGBA, CL_UNSIGNED_INT16}, S16},                                          // XXX
        {cl_image_format{CL_RGBA, CL_UNSIGNED_INT32}, A1},                                           // XXX
        {cl_image_format{CL_RGBA, CL_HALF_FLOAT}, RGBA64}, {cl_image_format{CL_RGBA, CL_FLOAT}, A1}, // XXX
        // XXX { CL_BGRA, CL_UNORM_INT8} is also required by FULL_PROFILE, not EMBEDDED_PROFILE, but OpenCL-CTS requires
        // it for image-tests
        // XXX additional supported formats
        {cl_image_format{CL_YUYV_INTEL, CL_UNORM_INT8}, YUYV422R}};

static const std::unordered_map<cl_channel_order, ChannelOrder> channelOrders = {{CL_R, CHANNEL_RED},
    {CL_Rx, CHANNEL_REDx}, {CL_A, CHANNEL_ALPHA}, {CL_INTENSITY, CHANNEL_INTENSITY}, {CL_LUMINANCE, CHANNEL_LUMINANCE},
    {CL_RG, CHANNEL_RED_GREEN}, {CL_RGx, CHANNEL_RED_GREENx}, {CL_RA, CHANNEL_RED_ALPHA},
    {CL_RGB, CHANNEL_RED_GREEN_BLUE}, {CL_RGBx, CHANNEL_RED_GREEN_BLUEx}, {CL_RGBA, CHANNEL_RED_GREEN_BLUE_ALPHA},
    {CL_BGRA, CHANNEL_BLUE_GREEN_RED_ALPHA}, {CL_ARGB, CHANNEL_ALPHA_RED_GREEN_BLUE}, {CL_YUYV_INTEL, CHANNEL_Y_U_Y_V}};

static const std::unordered_map<cl_channel_type, ChannelType> channelTypes = {{CL_SNORM_INT8, CHANNEL_SNORM_INT8},
    {CL_SNORM_INT16, CHANNEL_SNORM_INT16}, {CL_UNORM_INT8, CHANNEL_UNORM_INT8}, {CL_UNORM_INT16, CHANNEL_UNORM_INT16},
    {CL_UNORM_SHORT_555, CHANNEL_UNORM_SHORT_555}, {CL_UNORM_SHORT_565, CHANNEL_UNORM_SHORT_565},
    {CL_UNORM_INT_101010, CHANNEL_UNORM_INT_101010}, {CL_SIGNED_INT8, CHANNEL_SIGNED_INT8},
    {CL_SIGNED_INT16, CHANNEL_SIGNED_INT16}, {CL_SIGNED_INT32, CHANNEL_SIGNED_INT32},
    {CL_UNSIGNED_INT8, CHANNEL_UNSIGNED_INT8}, {CL_UNSIGNED_INT16, CHANNEL_UNSIGNED_INT16},
    {CL_UNSIGNED_INT32, CHANNEL_UNSIGNED_INT32}, {CL_HALF_FLOAT, CHANNEL_HALF_FLOAT}, {CL_FLOAT, CHANNEL_FLOAT}};

static const std::unordered_map<cl_mem_object_type, ImageType> imageTypes = {{CL_MEM_OBJECT_IMAGE1D, IMAGE_1D},
    {CL_MEM_OBJECT_IMAGE1D_ARRAY, IMAGE_1D_ARRAY}, {CL_MEM_OBJECT_IMAGE1D_BUFFER, IMAGE_1D_BUFFER},
    {CL_MEM_OBJECT_IMAGE2D, IMAGE_2D}, {CL_MEM_OBJECT_IMAGE2D_ARRAY, IMAGE_2D_ARRAY},
    {CL_MEM_OBJECT_IMAGE3D, IMAGE_3D}};

Image::Image(
    Context* context, cl_mem_flags flags, const cl_image_format& imageFormat, const cl_image_desc& imageDescription) :
    Buffer(context, flags),
    channelOrder(channelOrders.at(imageFormat.image_channel_order)),
    channelType(channelTypes.at(imageFormat.image_channel_data_type)), textureType(supportedFormats.at(imageFormat)),
    imageType(imageTypes.at(imageDescription.image_type)), imageWidth(imageDescription.image_width),
    imageHeight(imageDescription.image_height), imageDepth(imageDescription.image_depth),
    imageArraySize(imageDescription.image_array_size), imageRowPitch(0), imageSlicePitch(0),
    numMipLevels(imageDescription.num_mip_levels), numSamples(imageDescription.num_samples)
{
}

cl_int Image::getImageInfo(
    cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    cl_image_format format{};
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
        if(imageType.isImageBuffer)
            return returnValue<cl_mem>(toBase(), param_value_size, param_value, param_value_size_ret);
        return returnValue<cl_mem>(nullptr, param_value_size, param_value, param_value_size_ret);
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

cl_int Image::enqueueRead(CommandQueue* commandQueue, cl_bool blockingRead, const size_t* origin, const size_t* region,
    size_t row_pitch, size_t slice_pitch, void* ptr, cl_uint numEventsInWaitList, const cl_event* waitList,
    cl_event* event)
{
    if(ptr == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Host-pointer cannot be NULL!");

    cl_int errcode = checkImageAccess(origin, region);
    if(errcode != CL_SUCCESS)
        return errcode;

    const size_t rowPitch = row_pitch != 0 ? row_pitch : region[0] * calculateElementSize();
    const size_t slicePitch = slice_pitch != 0 ? slice_pitch : rowPitch * region[1];

    errcode = checkImageSlices(region, rowPitch, slicePitch);
    if(errcode != CL_SUCCESS)
        return errcode;

    if(!hostReadable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot read non-readable image!");

    Event* e = createBufferActionEvent(commandQueue, CommandType::IMAGE_READ, numEventsInWaitList, waitList, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create image event!");
    }

    ImageAccess* access = newObject<ImageAccess>(this, ptr, false, origin, region);
    CHECK_ALLOCATION(access)
    access->hostRowPitch = rowPitch;
    access->hostSlicePitch = slicePitch;
    e->action.reset(access);

    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);

    if(errcode == CL_SUCCESS && blockingRead == CL_TRUE)
        errcode = e->waitFor();

    return e->setAsResultOrRelease(errcode, event);
}

cl_int Image::enqueueWrite(CommandQueue* commandQueue, cl_bool blockingWrite, const size_t* origin,
    const size_t* region, size_t row_pitch, size_t slice_pitch, const void* ptr, cl_uint numEventsInWaitList,
    const cl_event* waitList, cl_event* event)
{
    if(ptr == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Host-pointer cannot be NULL!");

    cl_int errcode = checkImageAccess(origin, region);
    if(errcode != CL_SUCCESS)
        return errcode;

    const size_t rowPitch = row_pitch != 0 ? row_pitch : region[0] * calculateElementSize();
    const size_t slicePitch = slice_pitch != 0 ? slice_pitch : rowPitch * region[1];

    errcode = checkImageSlices(region, rowPitch, slicePitch);
    if(errcode != CL_SUCCESS)
        return errcode;

    if(!hostWriteable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot write non-writable image!");

    Event* e = createBufferActionEvent(commandQueue, CommandType::IMAGE_WRITE, numEventsInWaitList, waitList, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create image event!");
    }

    ImageAccess* access = newObject<ImageAccess>(this, const_cast<void*>(ptr), true, origin, region);
    CHECK_ALLOCATION(access)
    access->hostRowPitch = rowPitch;
    access->hostSlicePitch = slicePitch;
    e->action.reset(access);

    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);

    if(errcode == CL_SUCCESS && blockingWrite == CL_TRUE)
        errcode = e->waitFor();

    return e->setAsResultOrRelease(errcode, event);
}

cl_int Image::enqueueCopyInto(CommandQueue* commandQueue, Image* destination, const size_t* srcOrigin,
    const size_t* dstOrigin, const size_t* region, cl_uint numEventsInWaitList, const cl_event* waitList,
    cl_event* event)
{
    if(context() != destination->context())
        return returnError(
            CL_INVALID_CONTEXT, __FILE__, __LINE__, "Context of source and destination images do not match!");

    if(channelOrder.id != destination->channelOrder.id || channelType.id != destination->channelType.id)
        return returnError(
            CL_INVALID_CONTEXT, __FILE__, __LINE__, "Channel formats  of source and destination images do not match!");

    cl_int errcode = checkImageAccess(srcOrigin, region);
    if(errcode != CL_SUCCESS)
        return errcode;
    errcode = checkImageAccess(dstOrigin, region);
    if(errcode != CL_SUCCESS)
        return errcode;

    if(destination == this)
    {
        bool xOverlaps = (srcOrigin[0] <= dstOrigin[0] && srcOrigin[0] + region[0] > dstOrigin[0]) ||
            (dstOrigin[0] <= srcOrigin[0] && dstOrigin[0] + region[0] > srcOrigin[0]);
        bool yOverlaps = false;
        if(imageType.numDimensions > 1 || imageType.isImageArray)
            yOverlaps = (srcOrigin[1] <= dstOrigin[1] && srcOrigin[1] + region[1] > dstOrigin[1]) ||
                (dstOrigin[1] <= srcOrigin[1] && dstOrigin[1] + region[1] > srcOrigin[1]);
        bool zOverlaps = false;
        if(imageType.numDimensions > 2 || (imageType.numDimensions == 2 && imageType.isImageArray))
            zOverlaps = (srcOrigin[2] <= dstOrigin[2] && srcOrigin[2] + region[2] > dstOrigin[2]) ||
                (dstOrigin[2] <= srcOrigin[2] && dstOrigin[2] + region[2] > srcOrigin[2]);

        if(xOverlaps && yOverlaps && zOverlaps)
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Source and destination regions overlap!");
    }

    if(!hostReadable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot copy from non-readable image!");
    if(!destination->hostWriteable)
        return returnError(CL_INVALID_OPERATION, __FILE__, __LINE__, "Cannot copy into non-writable image!");

    Event* e = createBufferActionEvent(commandQueue, CommandType::IMAGE_COPY, numEventsInWaitList, waitList, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create image event!");
    }

    ImageCopy* access = newObject<ImageCopy>(this, destination, srcOrigin, dstOrigin, region);
    CHECK_ALLOCATION(access)
    e->action.reset(access);

    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

cl_int Image::enqueueFill(CommandQueue* commandQueue, const void* color, const size_t* origin, const size_t* region,
    cl_uint numEventsInWaitList, const cl_event* waitList, cl_event* event)
{
    if(color == nullptr)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Color to fill is NULL!");

    cl_int errcode = checkImageAccess(origin, region);
    if(errcode != CL_SUCCESS)
        return errcode;

    Event* e = createBufferActionEvent(commandQueue, CommandType::IMAGE_FILL, numEventsInWaitList, waitList, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create image event!");
    }

    ImageFill* access = newObject<ImageFill>(this, color, origin, region);
    CHECK_ALLOCATION(access)
    e->action.reset(access);

    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

cl_int Image::enqueueCopyFromToBuffer(CommandQueue* commandQueue, Buffer* buffer, const size_t* origin,
    const size_t* region, const size_t bufferOffset, bool copyIntoImage, cl_uint numEventsInWaitList,
    const cl_event* waitList, cl_event* event)
{
    if(context() != buffer->context())
        return returnError(CL_INVALID_CONTEXT, __FILE__, __LINE__, "Context of image and buffer do not match!");
    //"CL_INVALID_MEM_OBJECT [...] or if dst_image is a 1D image buffer object created from src_buffer."
    if(deviceBuffer && buffer->deviceBuffer)
        return returnError(
            CL_INVALID_MEM_OBJECT, __FILE__, __LINE__, "Cannot copy between image and buffer using the same data!");

    cl_int errcode = checkImageAccess(origin, region);
    if(errcode != CL_SUCCESS)
        return errcode;

    if(exceedsLimits<size_t>(
           region[0] * region[1] * region[2] * calculateElementSize() + bufferOffset, 1, buffer->hostSize))
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "The region copied exceeds the buffer size!");

    Event* e = createBufferActionEvent(
        commandQueue, CommandType::IMAGE_COPY_TO_BUFFER, numEventsInWaitList, waitList, &errcode);
    if(e == nullptr)
    {
        return returnError(errcode, __FILE__, __LINE__, "Failed to create image event!");
    }

    ImageCopyBuffer* access = newObject<ImageCopyBuffer>(this, buffer, copyIntoImage, origin, region, bufferOffset);
    CHECK_ALLOCATION(access)
    e->action.reset(access);

    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);
    return e->setAsResultOrRelease(errcode, event);
}

void* Image::enqueueMap(CommandQueue* commandQueue, cl_bool blockingMap, cl_map_flags mapFlags, const size_t* origin,
    const size_t* region, size_t* rowPitchOutput, size_t* slicePitchOutput, cl_uint numEventsInWaitList,
    const cl_event* waitList, cl_event* event, cl_int* errcode_ret)
{
    if(!hostReadable && hasFlag<cl_mem_flags>(mapFlags, CL_MAP_READ))
        return returnError<void*>(
            CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Cannot read from not host-readable image!");
    if(!hostWriteable &&
        (hasFlag<cl_mem_flags>(mapFlags, CL_MAP_WRITE) ||
            hasFlag<cl_mem_flags>(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION)))
        return returnError<void*>(
            CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Cannot write to not host-writeable image!");

    if(rowPitchOutput == nullptr)
        return returnError<void*>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Image row-pitch output pointer is NULL!");
    if((imageType.isImageArray || imageType.numDimensions > 2) && slicePitchOutput == nullptr)
        return returnError<void*>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Image slice-pitch output pointer is NULL!");

    cl_int errcode = checkImageAccess(origin, region);
    if(errcode != CL_SUCCESS)
    {
        *errcode_ret = errcode;
        return nullptr;
    }

    // mapping = making the buffer available in the host-memory
    // our implementation does so automatically

    Event* e =
        createBufferActionEvent(commandQueue, CommandType::IMAGE_MAP, numEventsInWaitList, waitList, errcode_ret);
    if(e == nullptr)
    {
        return nullptr;
    }

    void* out_ptr = nullptr;
    //"If the image object is created with CL_MEM_USE_HOST_PTR [...]"
    if(useHostPtr && hostPtr != nullptr)
    {
        //"The host_ptr specified in clCreateImage is guaranteed to contain the latest bits [...]"
        memcpy(hostPtr, getDeviceHostPointerWithOffset(), hostSize);
        //"The pointer value returned by clEnqueueMapImage will be derived from the host_ptr specified when the image
        // object is created."
        out_ptr = hostPtr;
    }
    else
    {
        out_ptr = getDeviceHostPointerWithOffset();
    }

    std::list<MappingInfo>::const_iterator info = mappings.end();
    {
        // we need to already add the mapping here, otherwise queuing the clEnqueueUnmapMemObject might fail for the
        // memory are not being mapped yet, if the event handler did not process this event yet.
        std::lock_guard<std::mutex> mapGuard(mappingsLock);
        mappings.emplace_back(MappingInfo{reinterpret_cast<void*>(out_ptr), false,
            hasFlag<cl_map_flags>(mapFlags, CL_MAP_WRITE_INVALIDATE_REGION),
            /* only on direct match, i.e. if not combined with CL_MAP_WRITE(...) */
            mapFlags == CL_MAP_READ});
        info = --mappings.end();
    }

    ImageMapping* action = newObject<ImageMapping>(this, info, false, origin, region);
    CHECK_ALLOCATION_ERROR_CODE(action, errcode_ret, void*)
    e->action.reset(action);

    e->setEventWaitList(numEventsInWaitList, waitList);
    errcode = commandQueue->enqueueEvent(e);

    if(errcode == CL_SUCCESS && blockingMap == CL_TRUE)
        errcode = e->waitFor();

    errcode = e->setAsResultOrRelease(errcode, event);
    if(errcode != CL_SUCCESS)
        return returnError<void*>(errcode, errcode_ret, __FILE__, __LINE__, "Error releasing the event object!");

    RETURN_OBJECT(reinterpret_cast<void*>(out_ptr), errcode_ret)
}

TextureConfiguration Image::toTextureConfiguration() const
{
    // base pointer is in multiple of 4 KB
    BasicTextureSetup basicSetup(static_cast<uint32_t>(deviceBuffer->qpuPointer) / 4096, textureType);

    TextureAccessSetup accessSetup(textureType, static_cast<uint16_t>(imageWidth), static_cast<uint16_t>(imageHeight));
    /*
     * OpenCL 1.2 specification, page 305:
     * "The sampler-less read image functions behave exactly as the corresponding read image functions described in
     * section 6.12.14.2 that take integer coordinates and a sampler with filter mode set to CLK_FILTER_NEAREST ,
     * normalized coordinates set to CLK_NORMALIZED_COORDS_FALSE and addressing mode to CLK_ADDRESS_NONE."
     */
    accessSetup.setMagnificationFilter(TextureFilter::NEAREST);
    accessSetup.setMinificationFilter(TextureFilter::NEAREST);
    // leave wrap-modes at default (value 0 is repeat)

    ExtendedTextureSetup childDimensionSetup;
    ExtendedTextureSetup childOffsetSetup;

    if(imageType.isImageArray)
    {
        // TODO re-set "default" image-sizes?
        // TODO is child-images correct for image-arrays?
        // or do we need "normal" images with offset in base-address?
        childDimensionSetup.setParameterType(ParameterType::CHILD_DIMENSIONS);
        childDimensionSetup.setChildWidth(static_cast<uint16_t>(imageWidth));
        childDimensionSetup.setChildHeight(static_cast<uint16_t>(imageHeight));

        childOffsetSetup.setParameterType(ParameterType::CHILD_OFFSETS);
        // TODO child image offset (= image-slice?)
    }

    ChannelConfig config;
    config.setChannelOrder(channelOrder.id);
    config.setChannelType(channelType.id);

    return TextureConfiguration{basicSetup, accessSetup, childDimensionSetup, childOffsetSetup, config};
}

size_t Image::calculateElementSize() const
{
    return channelType.bytesPerComponent * channelOrder.numChannels;
}

cl_int Image::checkImageAccess(const size_t* origin, const size_t* region) const
{
    if((origin[2] != 0 && (imageType.numDimensions + static_cast<int>(imageType.isImageArray)) < 3) ||
        (origin[1] != 0 && (imageType.numDimensions + static_cast<int>(imageType.isImageArray)) < 2))
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Image origin for undefined dimension must be zero!");
    if(region[0] == 0 || region[1] == 0 || region[2] == 0)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Image region cannot be zero!");
    if((origin[2] != 1 && (imageType.numDimensions + static_cast<int>(imageType.isImageArray)) < 3) ||
        (origin[1] != 1 && (imageType.numDimensions + static_cast<int>(imageType.isImageArray)) < 2))
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Image region for undefined dimension must be one!");

    // 1D, 2D, 3D, 1D array, 2D array
    if(exceedsLimits<size_t>(origin[0] + region[0], 1, imageWidth))
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Pixel range accessed exceeds the image-width!");
    if(imageType.numDimensions > 1)
    {
        // 2D, 2D array, 3D
        if(exceedsLimits<size_t>(origin[1] + region[1], 1, imageHeight))
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Pixel range accessed exceeds the image-height!");
        // 3D
        if(imageType.numDimensions > 2 && exceedsLimits<size_t>(origin[2] + region[2], 1, imageDepth))
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Pixel range accessed exceeds the image-depth!");
        // 2D array
        if(imageType.numDimensions == 2 && imageType.isImageArray &&
            exceedsLimits<size_t>(origin[2] + region[2], 1, imageArraySize))
            return returnError(
                CL_INVALID_VALUE, __FILE__, __LINE__, "Pixel range accessed exceeds the image-array size!");
    }
    // 1D array
    else if(exceedsLimits<size_t>(origin[1] + region[1], 1, imageArraySize))
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Pixel range accessed exceeds the image-depth!");

    // special handling for YUYV images
    if(channelOrder.id == CHANNEL_Y_U_Y_V.id)
    {
        // x-coordinate of origin and region (ergo the width) must be even, since two pixels are packed into one value
        if(origin[0] % 2 != 0 || region[0] % 2 != 0)
            return returnError(CL_INVALID_VALUE, __FILE__, __LINE__,
                "For YUV-images, the origin x-coordinate as well as width of the area accessed need to be even "
                "pixels!");
    }
    return CL_SUCCESS;
}

CHECK_RETURN cl_int Image::checkImageSlices(
    const size_t* region, const size_t row_pitch, const size_t slice_pitch) const
{
    if(exceedsLimits<size_t>(region[0] * calculateElementSize(), 0, row_pitch))
        return returnError(
            CL_INVALID_VALUE, __FILE__, __LINE__, "Row pitch of the region is smaller than the width accessed!");
    if(imageType.numDimensions < 3 && !imageType.isImageArray && slice_pitch != 0)
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Slice pitch must be zero for 1D images!");
    // TODO what to check for 1D arrays?
    else if(imageType.numDimensions >= 2 && exceedsLimits<size_t>(region[1] * row_pitch, 0, slice_pitch))
        return returnError(
            CL_INVALID_VALUE, __FILE__, __LINE__, "Slice pitch of the region is smaller than the height accessed!");

    return CL_SUCCESS;
}

Sampler::Sampler(Context* context, bool normalizeCoords, cl_addressing_mode addressingMode, cl_filter_mode filterMode) :
    HasContext(context), normalized_coords(normalizeCoords), addressing_mode(addressingMode), filter_mode(filterMode)
{
}

Sampler::~Sampler() noexcept = default;

cl_int Sampler::getInfo(
    cl_sampler_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    switch(param_name)
    {
    case CL_SAMPLER_REFERENCE_COUNT:
        return returnValue<cl_uint>(referenceCount, param_value_size, param_value, param_value_size_ret);
    case CL_SAMPLER_CONTEXT:
        return returnValue<cl_context>(context()->toBase(), param_value_size, param_value, param_value_size_ret);
    case CL_SAMPLER_NORMALIZED_COORDS:
        return returnValue<cl_bool>(
            static_cast<cl_bool>(normalized_coords), param_value_size, param_value, param_value_size_ret);
    case CL_SAMPLER_ADDRESSING_MODE:
        return returnValue<cl_addressing_mode>(addressing_mode, param_value_size, param_value, param_value_size_ret);
    case CL_SAMPLER_FILTER_MODE:
        return returnValue<cl_filter_mode>(filter_mode, param_value_size, param_value, param_value_size_ret);
    }

    return returnError(
        CL_INVALID_VALUE, __FILE__, __LINE__, buildString("Invalid cl_sampler_info value %d", param_name));
}

static bool check_image_format(const cl_image_format* format)
{
    if(format == nullptr)
        return false;

    for(const auto& pair : supportedFormats)
    {
        if(pair.first.image_channel_data_type == format->image_channel_data_type &&
            pair.first.image_channel_order == format->image_channel_order)
            return true;
    }

    return false;
}

static size_t calculate_image_size(const Image& img)
{
    size_t image_size = 0;
    // see OpenCL 1.2 standard page 92
    switch(img.imageType.id)
    {
    case CL_MEM_OBJECT_IMAGE1D:
    case CL_MEM_OBJECT_IMAGE1D_BUFFER:
        image_size = img.imageRowPitch;
        break;
    case CL_MEM_OBJECT_IMAGE2D:
        image_size = img.imageRowPitch * img.imageHeight;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        image_size = img.imageSlicePitch * img.imageDepth;
        break;
    case CL_MEM_OBJECT_IMAGE1D_ARRAY:
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        image_size = img.imageSlicePitch * img.imageArraySize;
        break;
    }
    return image_size;
}

ImageAccess::ImageAccess(
    Image* image, void* hostPtr, bool writeImage, const std::size_t origin[3], const std::size_t region[3]) :
    image(image),
    hostPointer(hostPtr), writeToImage(writeImage), hostRowPitch(0), hostSlicePitch(0)
{
    memcpy(this->origin.data(), origin, 3 * sizeof(size_t));
    memcpy(this->region.data(), region, 3 * sizeof(size_t));
}

ImageAccess::~ImageAccess() = default;

cl_int ImageAccess::operator()()
{
    if(writeToImage)
        image->accessor->writePixelData(origin, region, hostPointer, hostRowPitch, hostSlicePitch);
    else
        image->accessor->readPixelData(origin, region, hostPointer, hostRowPitch, hostSlicePitch);
    return CL_SUCCESS;
}

ImageCopy::ImageCopy(Image* src, Image* dst, const std::size_t srcOrigin[3], const std::size_t dstOrigin[3],
    const std::size_t region[3]) :
    source(src),
    destination(dst)
{
    memcpy(this->sourceOrigin.data(), srcOrigin, 3 * sizeof(size_t));
    memcpy(this->destOrigin.data(), dstOrigin, 3 * sizeof(size_t));
    memcpy(this->region.data(), region, 3 * sizeof(size_t));
}

ImageCopy::~ImageCopy() = default;

cl_int ImageCopy::operator()()
{
    if(TextureAccessor::copyPixelData(*source->accessor, *destination->accessor, sourceOrigin, destOrigin, region))
        return CL_SUCCESS;
    else
        return CL_INVALID_OPERATION;
}

ImageFill::ImageFill(Image* img, const void* color, const std::size_t origin[3], const std::size_t region[3]) :
    image(img)
{
    //"The fill color is a four component RGBA [...]"
    // TODO see sections 6.12.14 and 8.3. from the OpenCL 1.2 specification for conversion
    this->fillColor.resize(img->calculateElementSize());
    memcpy(this->fillColor.data(), color, img->calculateElementSize());

    memcpy(this->origin.data(), origin, 3 * sizeof(size_t));
    memcpy(this->region.data(), region, 3 * sizeof(size_t));
}

ImageFill::~ImageFill() = default;

cl_int ImageFill::operator()()
{
    image->accessor->fillPixelData(origin, region, fillColor.data());
    return CL_SUCCESS;
}

ImageCopyBuffer::ImageCopyBuffer(Image* image, Buffer* buffer, bool copyIntoImage, const std::size_t imgOrigin[3],
    const std::size_t region[3], const size_t bufferOffset) :
    image(image),
    buffer(buffer), copyIntoImage(copyIntoImage), bufferOffset(bufferOffset)
{
    memcpy(this->imageOrigin.data(), imgOrigin, 3 * sizeof(size_t));
    memcpy(this->imageRegion.data(), region, 3 * sizeof(size_t));
}

ImageCopyBuffer::~ImageCopyBuffer() = default;

cl_int ImageCopyBuffer::operator()()
{
    uintptr_t hostPtr = reinterpret_cast<uintptr_t>(buffer->deviceBuffer->hostPointer) + bufferOffset;
    // OpenCL 1.2 specifies the region copied to be width [* height] [* depth], therefore the pitches match the sizes
    // (OpenCL 1.2 specification, page 111)
    const size_t rowPitch = imageRegion[0] * image->calculateElementSize();
    const size_t slicePitch = imageRegion[0] * imageRegion[1] * image->calculateElementSize();
    if(copyIntoImage)
        image->accessor->writePixelData(
            imageOrigin, imageRegion, reinterpret_cast<void*>(hostPtr), rowPitch, slicePitch);
    else
        image->accessor->readPixelData(
            imageOrigin, imageRegion, reinterpret_cast<void*>(hostPtr), rowPitch, slicePitch);

    return CL_SUCCESS;
}

ImageMapping::ImageMapping(Image* image, std::list<MappingInfo>::const_iterator mappingInfo, bool isUnmap,
    const std::size_t origin[3], const std::size_t region[3]) :
    BufferMapping(image, mappingInfo, isUnmap)
{
    memcpy(this->origin.data(), origin, 3 * sizeof(size_t));
    memcpy(this->region.data(), region, 3 * sizeof(size_t));
}

ImageMapping::~ImageMapping() = default;

size_t hash_cl_image_format::operator()(const cl_image_format& format) const noexcept
{
    ChannelConfig config;
    config.setChannelOrder(format.image_channel_order);
    config.setChannelType(format.image_channel_data_type);
    return config.value;
}

bool equal_cl_image_format::operator()(const cl_image_format& f1, const cl_image_format& f2) const noexcept
{
    return f1.image_channel_data_type == f2.image_channel_data_type && f1.image_channel_order == f2.image_channel_order;
}

/*!
 * OpenCL 1.2 specification, pages 91+:
 *
 *  A 1D image, 1D image buffer, 1D image array, 2D image, 2D image array and 3D image object can be created using the
 * following function:
 *
 *  \param context is a valid OpenCL context on which the image object is to be created.
 *
 *  \param flags is a bit-field that is used to specify allocation and usage information about the image memory object
 * being created and is described in table 5.3. For all image types except CL_MEM_OBJECT_IMAGE1D_BUFFER , if value
 * specified for flags is 0, the default is used which is CL_MEM_READ_WRITE . For CL_MEM_OBJECT_IMAGE1D_BUFFER image
 * type, if the CL_MEM_READ_WRITE , CL_MEM_READ_ONLY or CL_MEM_WRITE_ONLY values are not specified in flags, they are
 * inherited from the corresponding memory access qualifers associated with buffer. The CL_MEM_USE_HOST_PTR ,
 *  CL_MEM_ALLOC_HOST_PTR and CL_MEM_COPY_HOST_PTR values cannot be specified in flags but are inherited from the
 * corresponding memory access qualifiers associated with buffer. If CL_MEM_COPY_HOST_PTR is specified in the memory
 * access qualifier values associated with buffer it does not imply any additional copies when the sub-buffer is
 * created from buffer. If the CL_MEM_HOST_WRITE_ONLY , CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS values are not
 * specified in flags, they are inherited from the corresponding memory access qualifiers associated with buffer.
 *
 *  \param image_format is a pointer to a structure that describes format properties of the image to be allocated.
 *  Refer to section 5.3.1.1 for a detailed description of the image format descriptor.
 *
 *  \param image_desc is a pointer to a structure that describes type and dimensions of the image to be allocated.
 *  Refer to section 5.3.1.2 for a detailed description of the image descriptor.
 *
 *  \param host_ptr is a pointer to the image data that may already be allocated by the application.
 *  Refer to table below for a description of how large the buffer that host_ptr points to must be.
 *  For a 3D image or 2D image array, the image data specified by host_ptr is stored as a linear sequence of adjacent 2D
 * image slices or 2D images respectively. Each 2D image is a linear sequence of adjacent scanlines. Each scanline is a
 * linear sequence of image elements. For a 2D image, the image data specified by host_ptr is stored as a linear
 * sequence of adjacent scanlines. Each scanline is a linear sequence of image elements. For a 1D image array, the
 * image data specified by host_ptr is stored as a linear sequence of adjacent 1D images respectively. Each 1D image or
 * 1D image buffer is a single scanline which is a linear sequence of adjacent elements.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clCreateImage returns a valid non-zero image object created and the errcode_ret is set to CL_SUCCESS if the
 * image object is created successfully. Otherwise, it returns a NULL value with one of the following error values
 * returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if values specified in flags are not valid.
 *  - CL_INVALID_IMAGE_FORMAT_DESCRIPTOR if values specified in image_format are not valid or if image_format is NULL .
 *  - CL_INVALID_IMAGE_DESCRIPTOR if values specified in image_desc are not valid or if image_desc is NULL .
 *  - CL_INVALID_IMAGE_SIZE if image dimensions specified in image_desc exceed the minimum maximum image dimensions
 * described in table 4.3 for all devices in context.
 *  - CL_INVALID_HOST_PTR if host_ptr is NULL and CL_MEM_USE_HOST_PTR or CL_MEM_COPY_HOST_PTR are set in flags or if
 * host_ptr is not NULL but CL_MEM_COPY_HOST_PTR or CL_MEM_USE_HOST_PTR are not set in flags.
 *  - CL_INVALID_VALUE if a 1D image buffer is being created and the buffer object was created with CL_MEM_WRITE_ONLY
 * and flags specifies CL_MEM_READ_WRITE or CL_MEM_READ_ONLY , or if the buffer object was created with CL_MEM_READ_ONLY
 * and flags specifies CL_MEM_READ_WRITE or CL_MEM_WRITE_ONLY , or if flags specifies CL_MEM_USE_HOST_PTR or
 * CL_MEM_ALLOC_HOST_PTR or CL_MEM_COPY_HOST_PTR .
 *  - CL_INVALID_VALUE if a 1D image buffer is being created and the buffer object was created with
 * CL_MEM_HOST_WRITE_ONLY and flags specifies CL_MEM_HOST_READ_ONLY , or if the buffer object was created with
 * CL_MEM_HOST_READ_ONLY and flags specifies CL_MEM_HOST_WRITE_ONLY , or if the buffer object was created with
 * CL_MEM_HOST_NO_ACCESS and flags specifies CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_WRITE_ONLY .
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if the image_format is not supported.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for image object.
 *  - CL_INVALID_OPERATION if there are no devices in context that support images (i.e. CL_DEVICE_IMAGE_SUPPORT
 * specified in table 4.3 is CL_FALSE ).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_mem VC4CL_FUNC(clCreateImage)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format,
    const cl_image_desc* image_desc, void* host_ptr, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_mem", clCreateImage, "cl_context", context, "cl_mem_flags", flags,
        "const cl_image_format*", image_format, "const cl_image_desc*", image_desc, "void*", host_ptr, "cl_int*",
        errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_mem)
#ifndef IMAGE_SUPPORT
    return returnError<cl_mem>(CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Image support is not enabled!");
#endif

    if(moreThanOneMemoryAccessFlagSet(flags))
        return returnError<cl_mem>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "More than one memory-access flag set!");
    if(moreThanOneHostAccessFlagSet(flags))
        return returnError<cl_mem>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "More than one host-access flag set!");

    if(image_format == nullptr)
        return returnError<cl_mem>(
            CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, errcode_ret, __FILE__, __LINE__, "Image format is not set!");
    if(!check_image_format(image_format))
        return returnError<cl_mem>(CL_IMAGE_FORMAT_NOT_SUPPORTED, errcode_ret, __FILE__, __LINE__,
            buildString("Unsupported image format (type: %u, order: %u)!", image_format->image_channel_data_type,
                image_format->image_channel_order));

    if(image_desc == nullptr)
        return returnError<cl_mem>(
            CL_INVALID_IMAGE_DESCRIPTOR, errcode_ret, __FILE__, __LINE__, "Image description is not set!");
    if(image_desc->image_type != CL_MEM_OBJECT_IMAGE1D && image_desc->image_type != CL_MEM_OBJECT_IMAGE1D_ARRAY &&
        image_desc->image_type != CL_MEM_OBJECT_IMAGE1D_BUFFER && image_desc->image_type != CL_MEM_OBJECT_IMAGE2D &&
        image_desc->image_type != CL_MEM_OBJECT_IMAGE2D_ARRAY && image_desc->image_type != CL_MEM_OBJECT_IMAGE3D)
        return returnError<cl_mem>(CL_INVALID_IMAGE_DESCRIPTOR, errcode_ret, __FILE__, __LINE__, "Invalid image-type!");
    if(image_desc->image_width > kernel_config::MAX_IMAGE_DIMENSION)
        return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__,
            buildString("Image width (%u) exceeds supported maximum (%u)!", image_desc->image_width,
                kernel_config::MAX_IMAGE_DIMENSION));
    if((image_desc->image_type == CL_MEM_OBJECT_IMAGE1D_ARRAY ||
           image_desc->image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) &&
        image_desc->image_array_size > kernel_config::MAX_IMAGE_DIMENSION)
        return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__,
            buildString("Image array size (%u) exceeds supported maximum (%u)!", image_desc->image_array_size,
                kernel_config::MAX_IMAGE_DIMENSION));
    if((image_desc->image_type == CL_MEM_OBJECT_IMAGE2D || image_desc->image_type == CL_MEM_OBJECT_IMAGE3D ||
           image_desc->image_type == CL_MEM_OBJECT_IMAGE2D_ARRAY) &&
        image_desc->image_height > kernel_config::MAX_IMAGE_DIMENSION)
        return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__,
            buildString("Image height (%u) exceeds supported maximum (%u)!", image_desc->image_height,
                kernel_config::MAX_IMAGE_DIMENSION));
    if(image_desc->image_type == CL_MEM_OBJECT_IMAGE3D && image_desc->image_depth > kernel_config::MAX_IMAGE_DIMENSION)
        return returnError<cl_mem>(CL_INVALID_IMAGE_SIZE, errcode_ret, __FILE__, __LINE__,
            buildString("Image depth (%u) exceeds supported maximum (%u)!", image_desc->image_depth,
                kernel_config::MAX_IMAGE_DIMENSION));
    if(host_ptr == nullptr && (image_desc->image_row_pitch != 0 || image_desc->image_slice_pitch != 0))
        return returnError<cl_mem>(CL_INVALID_IMAGE_DESCRIPTOR, errcode_ret, __FILE__, __LINE__,
            "Image row and slice pitches need to be zero, if no host-pointer is set!");
    if(image_desc->num_mip_levels != 0 || image_desc->num_samples != 0)
        return returnError<cl_mem>(CL_INVALID_IMAGE_DESCRIPTOR, errcode_ret, __FILE__, __LINE__,
            "Mip-map levels and sampler are not supported!");
    if((image_desc->image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER) != (image_desc->buffer != nullptr))
        return returnError<cl_mem>(CL_INVALID_IMAGE_DESCRIPTOR, errcode_ret, __FILE__, __LINE__,
            "Buffer must be set if, and only if, the image-type is CL_MEM_OBJECT_IMAGE1D_BUFFER!");

    Buffer* buffer = toType<Buffer>(image_desc->buffer);

    if(buffer != nullptr)
    {
        const cl_mem_flags bufferFlags = buffer->getMemFlags();
        if(hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_WRITE_ONLY) &&
            (hasFlag<cl_mem_flags>(flags, CL_MEM_READ_ONLY) || hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE)))
            return returnError<cl_mem>(
                CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Memory flags of image and buffer do not match!");
        if(hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_READ_ONLY) &&
            (hasFlag<cl_mem_flags>(flags, CL_MEM_WRITE_ONLY) || hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE)))
            return returnError<cl_mem>(
                CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Memory flags of image and buffer do not match!");
        if(hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_USE_HOST_PTR) ||
            hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_ALLOC_HOST_PTR) ||
            hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_COPY_HOST_PTR))
            return returnError<cl_mem>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
                "Cannot use/copy/allocate host pointer for image, when source buffer is set!");
        if((hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_HOST_WRITE_ONLY) &&
               hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_READ_ONLY)) ||
            (hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_HOST_READ_ONLY) &&
                hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_WRITE_ONLY)) ||
            (hasFlag<cl_mem_flags>(bufferFlags, CL_MEM_HOST_NO_ACCESS) &&
                (hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_READ_ONLY) ||
                    hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_WRITE_ONLY))))
            return returnError<cl_mem>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
                "Host access flags of image and buffer do not match!");
    }

    /*
     * "For CL_MEM_OBJECT_IMAGE1D_BUFFER image type, if the CL_MEM_READ_WRITE, CL_MEM_READ_ONLY or CL_MEM_WRITE_ONLY
     * values are not specified in flags, they are inherited from the corresponding memory access qualifers associated
     * with buffer.The	CL_MEM_USE_HOST_PTR , CL_MEM_ALLOC_HOST_PTR and CL_MEM_COPY_HOST_PTR values cannot be specified
     * in flags but are inherited from the corresponding memory access qualifiers associated with buffer. [...] If the
     * CL_MEM_HOST_WRITE_ONLY, CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS values are not specified in flags, they
     * are inherited from the corresponding memory access qualifiers associated with buffer."
     */
    if(image_desc->image_type == CL_MEM_OBJECT_IMAGE1D_BUFFER)
    {
        const cl_mem_flags bufferFlags = buffer->getMemFlags();
        flags |= bufferFlags & (CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR);
        if(!hasFlag<cl_mem_flags>(flags, CL_MEM_READ_WRITE | CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY))
            flags |= bufferFlags & (CL_MEM_READ_WRITE | CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY);
        if(!hasFlag<cl_mem_flags>(flags, CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS))
            flags |= bufferFlags & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS);
    }
    //"For all image types except CL_MEM_OBJECT_IMAGE1D_BUFFER , if value specified for flags is 0, the default is used
    // which is CL_MEM_READ_WRITE."
    else if(flags == 0)
        flags = CL_MEM_READ_WRITE;

    if(host_ptr == nullptr &&
        (hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR) || hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR)))
        return returnError<cl_mem>(CL_INVALID_HOST_PTR, errcode_ret, __FILE__, __LINE__,
            "Usage of host-pointer specified in flags but no host-buffer given!");
    if(host_ptr != nullptr &&
        !(hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR) || hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR)))
        return returnError<cl_mem>(CL_INVALID_HOST_PTR, errcode_ret, __FILE__, __LINE__,
            "Host pointer given, but not used according to flags!");

    Image* image = newOpenCLObject<Image>(toType<Context>(context), flags, *image_format, *image_desc);
    CHECK_ALLOCATION_ERROR_CODE(image, errcode_ret, cl_mem)

    image->accessor.reset(TextureAccessor::createTextureAccessor(*image));
    if(image->accessor == nullptr)
    {
        ignoreReturnValue(image->release(), __FILE__, __LINE__, "Already errored");
        return returnError<cl_mem>(CL_IMAGE_FORMAT_NOT_SUPPORTED, errcode_ret, __FILE__, __LINE__,
            "Failed to create a texture-accessor for the image-format!");
    }
    cl_int errcode = image->accessor->checkAndApplyPitches(image_desc->image_row_pitch, image_desc->image_slice_pitch);
    if(errcode != CL_SUCCESS)
    {
        ignoreReturnValue(image->release(), __FILE__, __LINE__, "Already errored");
        return returnError<cl_mem>(errcode, errcode_ret, __FILE__, __LINE__, "Invalid image row or slice pitches!");
    }

    // calculate buffer size
    size_t size = calculate_image_size(*image);
    if(size == 0)
    {
        ignoreReturnValue(image->release(), __FILE__, __LINE__, "Already errored");
        return returnError<cl_mem>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Image has no size!");
    }

    if(buffer != nullptr)
        image->deviceBuffer = buffer->deviceBuffer;
    else
        image->deviceBuffer.reset(mailbox()->allocateBuffer(static_cast<unsigned>(size)));
    if(!image->deviceBuffer)
    {
        ignoreReturnValue(image->release(), __FILE__, __LINE__, "Already errored");
        return returnError<cl_mem>(CL_OUT_OF_RESOURCES, errcode_ret, __FILE__, __LINE__,
            buildString("Failed to allocate enough device memory (%u)!", size));
    }

    // TODO are these correct??
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_USE_HOST_PTR))
        image->setUseHostPointer(host_ptr, size);
    else if(hasFlag<cl_mem_flags>(flags, CL_MEM_ALLOC_HOST_PTR))
        image->setAllocateHostPointer(size);
    if(hasFlag<cl_mem_flags>(flags, CL_MEM_COPY_HOST_PTR))
    {
        //"CL_MEM_COPY_HOST_PTR can be used with CL_MEM_ALLOC_HOST_PTR"
        image->setCopyHostPointer(host_ptr, size);
        // copy image data correctly
        std::array<size_t, 3> origin{0, 0, 0};
        // TODO image-arrays?!
        std::array<size_t, 3> region{image->imageWidth, image->imageHeight, image->imageDepth};
        // TODO not for image-buffers?
        image->accessor->writePixelData(origin, region, host_ptr, image->imageRowPitch, image->imageSlicePitch);
    }

    image->setHostSize();

    RETURN_OBJECT(image->toBase(), errcode_ret)
}

cl_mem VC4CL_FUNC(clCreateImage2D)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format,
    size_t image_width, size_t image_height, size_t image_row_pitch, void* host_ptr, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_mem", clCreateImage2D, "cl_context", context, "cl_mem_flags", flags,
        "const cl_image_format*", image_format, "size_t", image_width, "size_t", image_height, "size_t",
        image_row_pitch, "void*", host_ptr, "cl_int*", errcode_ret);
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
    desc.buffer = nullptr;
    return VC4CL_FUNC(clCreateImage)(context, flags, image_format, &desc, host_ptr, errcode_ret);
}

cl_mem VC4CL_FUNC(clCreateImage3D)(cl_context context, cl_mem_flags flags, const cl_image_format* image_format,
    size_t image_width, size_t image_height, size_t image_depth, size_t image_row_pitch, size_t image_slice_pitch,
    void* host_ptr, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_mem", clCreateImage3D, "cl_context", context, "cl_mem_flags", flags,
        "const cl_image_format*", image_format, "size_t", image_width, "size_t", image_height, "size_t", image_depth,
        "size_t", image_row_pitch, "size_t", image_slice_pitch, "void*", host_ptr, "cl_int*", errcode_ret);
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
    desc.buffer = nullptr;
    return VC4CL_FUNC(clCreateImage)(context, flags, image_format, &desc, host_ptr, errcode_ret);
}

/*!
 * OpenCL 1.2 specification, pages 97+:
 *
 *  Can be used to get the list of image formats supported by an OpenCL implementation when the following information
 * about an image memory object is specified:
 *  - Context
 *  - Image type – 1D, 2D, or 3D image, 1D image buffer, 1D or 2D image array.
 *  - Image object allocation information
 *
 *  clGetSupportedImageFormats returns a union of image formats supported by all devices in the context.
 *
 *  \param context is a valid OpenCL context on which the image object(s) will be created.
 *
 *  \param flags is a bit-field that is used to specify allocation and usage information about the image memory object
 * being created and is described in table 5.3.
 *
 *  \param image_type describes the image type and must be either CL_MEM_OBJECT_IMAGE1D ,CL_MEM_OBJECT_IMAGE1D_BUFFER ,
 * CL_MEM_OBJECT_IMAGE2D ,CL_MEM_OBJECT_IMAGE3D , CL_MEM_OBJECT_IMAGE1D_ARRAY or CL_MEM_OBJECT_IMAGE2D_ARRAY .
 *
 *  \param num_entries specifies the number of entries that can be returned in the memory location given by
 * image_formats.
 *
 *  \param image_formats is a pointer to a memory location where the list of supported image formats are returned. Each
 * entry describes a cl_image_format structure supported by the OpenCL implementation. If image_formats is NULL , it is
 * ignored.
 *
 *  \param num_image_formats is the actual number of supported image formats for a specific context and values specified
 * by flags. If num_image_formats is NULL , it is ignored.
 *
 *  \return clGetSupportedImageFormats returns CL_SUCCESS if the function is executed successfully. Otherwise, it
 * returns one of the following errors:
 *  - CL_INVALID_CONTEXT if context is not a valid context.
 *  - CL_INVALID_VALUE if flags or image_type are not valid, or if num_entries is 0 and image_formats is not NULL.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *
 *  If CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_TRUE , the values assigned to CL_DEVICE_MAX_READ_IMAGE_ARGS
 * , CL_DEVICE_MAX_WRITE_IMAGE_ARGS ,CL_DEVICE_IMAGE2D_MAX_WIDTH , CL_DEVICE_IMAGE2D_MAX_HEIGHT ,
 *  CL_DEVICE_IMAGE3D_MAX_WIDTH , CL_DEVICE_IMAGE3D_MAX_HEIGHT , CL_DEVICE_IMAGE3D_MAX_DEPTH and CL_DEVICE_MAX_SAMPLERS
 * by the implementation must be greater than or equal to the minimum values specified in table 4.3.
 */
cl_int VC4CL_FUNC(clGetSupportedImageFormats)(cl_context context, cl_mem_flags flags, cl_mem_object_type image_type,
    cl_uint num_entries, cl_image_format* image_formats, cl_uint* num_image_formats)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetSupportedImageFormats, "cl_context", context, "cl_mem_flags", flags,
        "cl_mem_object_type", image_type, "cl_uint", num_entries, "cl_image_format*", image_formats, "cl_uint*",
        num_image_formats);
    CHECK_CONTEXT(toType<Context>(context))
#ifdef IMAGE_SUPPORT
    if((num_entries == 0) != (image_formats == nullptr))
        return returnError(CL_INVALID_VALUE, __FILE__, __LINE__, "Output parameters are empty!");

    if(num_entries >= supportedFormats.size())
    {
        for(const auto& pair : supportedFormats)
        {
            *image_formats = pair.first;
            ++image_formats;
        }
    }
    if(num_image_formats != nullptr)
        *num_image_formats = static_cast<cl_uint>(supportedFormats.size());
#endif
    return CL_SUCCESS;
}

/*!
 * OpenCL 1.2 specification, pages 100+:
 *
 *  The following functions enqueue commands to read from an image or image array object to host memory or write to an
 * image or image array object from host memory.
 *
 *  \param command_queue refers to the command-queue in which the read / write command will be queued. command_queue and
 * image must be created with the same OpenCL context.
 *
 *  \param image refers to a valid image or image array object.
 *
 *  \param blocking_read and blocking_write indicate if the read and write operations are blocking or non-blocking.
 *  If blocking_read is CL_TRUE i.e. the read command is blocking, clEnqueueReadImage does not return until the buffer
 * data has been read and copied into memory pointed to by ptr. If blocking_read is CL_FALSE i.e. the read command is
 * non-blocking, clEnqueueReadImage queues a non-blocking read command and returns. The contents of the buffer that ptr
 * points to cannot be used until the read command has completed. The event argument returns an event object which can
 * be used to query the execution status of the read command. When the read command has completed, the contents of the
 * buffer that ptr points to can be used by the application.
 *
 *  \param origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If image is a 2D image
 * object, origin[2] must be 0. If image is a 1D image or 1D image buffer object, origin[1] and origin[2] must be 0. If
 * image is a 1D image array object, origin[2] must be 0. If image is a 1D image array object, origin[1] describes the
 * image index in the 1D image array. If image is a 2D image array object, origin[2] describes the image index in the
 * 2D image array.
 *
 *  \param region defines the (width, height, depth) in pixels of the 1D, 2D or 3D rectangle, the (width, height) in
 * pixels of the 2D rectangle and the number of images of a 2D image array or the (width) in pixels of the 1D rectangle
 * and the number of images of a 1D image array. If image is a 2D image object, region[2] must be 1. If image is a 1D
 * image or 1D image buffer object, region[1] and region[2] must be 1. If image is a 1D image array object, region[2]
 * must be 1. The values in region cannot be 0.
 *
 *  \param row_pitch in clEnqueueReadImage and input_row_pitch in clEnqueueWriteImage is the length of each row in
 * bytes. This value must be greater than or equal to the element size in bytes * width. If row_pitch (or
 * input_row_pitch) is set to 0, the appropriate row pitch is calculated based on the size of each element in bytes
 * multiplied by width.
 *
 *  \param slice_pitch in clEnqueueReadImage and input_slice_pitch in clEnqueueWriteImage is the size in bytes of the 2D
 * slice of the 3D region of a 3D image or each image of a 1D or 2D image array being read or written respectively. This
 * must be 0 if image is a 1D or 2D image. This value must be greater than or equal to row_pitch * height. If
 * slice_pitch (or input_slice_pitch) is set to 0, the appropriate slice pitch is calculated based on the row_pitch *
 * height.
 *
 *  \param ptr is the pointer to a buffer in host memory where image data is to be read from or to be written to.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular read / write command and can be used to query
 * or queue a wait for this particular command to complete. event can be NULL in which case it will not be possible for
 * the application to query the status of this command or queue a wait for this command to complete. If the
 * event_wait_list and the event arguments are not NULL , the event argument should not refer to an element of the
 * event_wait_list array.
 *
 *  \return clEnqueueReadImage and clEnqueueWriteImage return CL_SUCCESS if the function is executed successfully.
 * Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and image are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if image is not a valid image object.
 *  - CL_INVALID_VALUE if the region being read or written specified by origin and region is out of bounds or if ptr is
 * a NULL value.
 *  - CL_INVALID_VALUE if values in origin and region do not follow rules described in the argument description for
 * origin and region.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_INVALID_IMAGE_SIZE if image dimensions (image width, height, specified or compute row and/or slice pitch) for
 * image are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel order and data type) for image are not supported by
 * device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with image.
 *  - CL_INVALID_OPERATION if the device associated with command_queue does not support images (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_INVALID_OPERATION if clEnqueueReadImage is called on image which has been created with CL_MEM_HOST_WRITE_ONLY
 * or CL_MEM_HOST_NO_ACCESS .
 *  - CL_INVALID_OPERATION if clEnqueueWriteImage is called on image which has been created with CL_MEM_HOST_READ_ONLY
 * or CL_MEM_HOST_NO_ACCESS .
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the read and write operations are blocking and the execution
 * status of any of the events in event_wait_list is a negative integer value.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE: Calling clEnqueueReadImage to read a region of the image with the ptr argument value set to host_ptr +
 * (origin[2] * image slice pitch + origin[1] * image row pitch + origin[0] * bytes per pixel), where host_ptr is a
 * pointer to the memory region specified when the image being read is created with CL_MEM_USE_HOST_PTR , must meet the
 * following requirements in order to avoid undefined behavior:
 *  - All commands that use this image object have finished execution before the read command begins execution.
 *  - The row_pitch and slice_pitch argument values in clEnqueueReadImage must be set to the image row pitch and slice
 * pitch.
 *  - The image object is not mapped.
 *  - The image object is not used by any command-queue until the read command has finished execution.
 */
cl_int VC4CL_FUNC(clEnqueueReadImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read,
    const size_t* origin, const size_t* region, size_t row_pitch, size_t slice_pitch, void* ptr,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueReadImage, "cl_command_queue", command_queue, "cl_mem", image, "cl_bool",
        blocking_read, "const size_t*", origin, "const size_t*", region, "size_t", row_pitch, "size_t", slice_pitch,
        "void*", ptr, "cl_uint", num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Image>(image))
    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    return toType<Image>(image)->enqueueRead(toType<CommandQueue>(command_queue), blocking_read, origin, region,
        row_pitch, slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 100+:
 *
 *  The following functions enqueue commands to read from an image or image array object to host memory or write to an
 * image or image array object from host memory.
 *
 *  \param command_queue refers to the command-queue in which the read / write command will be queued. command_queue and
 * image must be created with the same OpenCL context.
 *
 *  \param image refers to a valid image or image array object.
 *
 *  \param blocking_read and blocking_write indicate if the read and write operations are blocking or non-blocking.
 *  If blocking_write is CL_TRUE , the OpenCL implementation copies the data referred to by ptr and enqueues the write
 * command in the command-queue. The memory pointed to by ptr can be reused by the application after the
 * clEnqueueWriteImage call returns. If blocking_write is CL_FALSE , the OpenCL implementation will use ptr to perform
 * a non-blocking write. As the write is non-blocking the implementation can return immediately. The memory pointed to
 * by ptr cannot be reused by the application after the call returns. The event argument returns an event object which
 * can be used to query the execution status of the write command. When the write command has completed, the memory
 * pointed to by ptr can then be reused by the application.
 *
 *  \param origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If image is a 2D image
 * object, origin[2] must be 0. If image is a 1D image or 1D image buffer object, origin[1] and origin[2] must be 0. If
 * image is a 1D image array object, origin[2] must be 0. If image is a 1D image array object, origin[1] describes the
 * image index in the 1D image array. If image is a 2D image array object, origin[2] describes the image index in the
 * 2D image array.
 *
 *  \param region defines the (width, height, depth) in pixels of the 1D, 2D or 3D rectangle, the (width, height) in
 * pixels of the 2D rectangle and the number of images of a 2D image array or the (width) in pixels of the 1D rectangle
 * and the number of images of a 1D image array. If image is a 2D image object, region[2] must be 1. If image is a 1D
 * image or 1D image buffer object, region[1] and region[2] must be 1. If image is a 1D image array object, region[2]
 * must be 1. The values in region cannot be 0.
 *
 *  \param row_pitch in clEnqueueReadImage and input_row_pitch in clEnqueueWriteImage is the length of each row in
 * bytes. This value must be greater than or equal to the element size in bytes * width. If row_pitch (or
 * input_row_pitch) is set to 0, the appropriate row pitch is calculated based on the size of each element in bytes
 * multiplied by width.
 *
 *  \param slice_pitch in clEnqueueReadImage and input_slice_pitch in clEnqueueWriteImage is the size in bytes of the 2D
 * slice of the 3D region of a 3D image or each image of a 1D or 2D image array being read or written respectively. This
 * must be 0 if image is a 1D or 2D image. This value must be greater than or equal to row_pitch * height. If
 * slice_pitch (or input_slice_pitch) is set to 0, the appropriate slice pitch is calculated based on the row_pitch *
 * height.
 *
 *  \param ptr is the pointer to a buffer in host memory where image data is to be read from or to be written to.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular read / write command and can be used to query
 * or queue a wait for this particular command to complete. event can be NULL in which case it will not be possible for
 * the application to query the status of this command or queue a wait for this command to complete. If the
 * event_wait_list and the event arguments are not NULL , the event argument should not refer to an element of the
 * event_wait_list array.
 *
 *  \return clEnqueueReadImage and clEnqueueWriteImage return CL_SUCCESS if the function is executed successfully.
 * Otherwise, it returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and image are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if image is not a valid image object.
 *  - CL_INVALID_VALUE if the region being read or written specified by origin and region is out of bounds or if ptr is
 * a NULL value.
 *  - CL_INVALID_VALUE if values in origin and region do not follow rules described in the argument description for
 * origin and region.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_INVALID_IMAGE_SIZE if image dimensions (image width, height, specified or compute row and/or slice pitch) for
 * image are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel order and data type) for image are not supported by
 * device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with image.
 *  - CL_INVALID_OPERATION if the device associated with command_queue does not support images (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_INVALID_OPERATION if clEnqueueReadImage is called on image which has been created with CL_MEM_HOST_WRITE_ONLY
 * or CL_MEM_HOST_NO_ACCESS .
 *  - CL_INVALID_OPERATION if clEnqueueWriteImage is called on image which has been created with CL_MEM_HOST_READ_ONLY
 * or CL_MEM_HOST_NO_ACCESS .
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the read and write operations are blocking and the execution
 * status of any of the events in event_wait_list is a negative integer value.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE: Calling clEnqueueWriteImage to update the latest bits in a region of the image with the ptr argument value set
 * to host_ptr + (origin[2] * image slice pitch + origin[1] * image row pitch + origin[0] * bytes per pixel), where
 * host_ptr is a pointer to the memory region specified when the image being written is created with
 * CL_MEM_USE_HOST_PTR , must meet the following requirements in order to avoid undefined behavior:
 *  - The host memory region being written contains the latest bits when the enqueued write command begins execution.
 *  - The input_row_pitch and input_slice_pitch argument values in clEnqueueWriteImage must be set to the image row
 * pitch and slice pitch.
 *  - The image object is not mapped.
 *  - The image object is not used by any command-queue until the write command has finished execution.
 */
cl_int VC4CL_FUNC(clEnqueueWriteImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write,
    const size_t* origin, const size_t* region, size_t input_row_pitch, size_t input_slice_pitch, const void* ptr,
    cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueWriteImage, "cl_command_queue", command_queue, "cl_mem", image, "cl_bool",
        blocking_write, "const size_t*", origin, "const size_t*", region, "size_t", input_row_pitch, "size_t",
        input_slice_pitch, "const void*", ptr, "cl_uint", num_events_in_wait_list, "const cl_event*", event_wait_list,
        "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Image>(image))
    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    return toType<Image>(image)->enqueueWrite(toType<CommandQueue>(command_queue), blocking_write, origin, region,
        input_row_pitch, input_slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 103+:
 *
 *  Enqueues a command to copy image objects. src_image and dst_image can be 1D, 2D, 3D image or a 1D, 2D image array
 * objects allowing us to perform the following actions:
 *  - Copy a 1D image object to a 1D image object.
 *  - Copy a 1D image object to a scanline of a 2D image object and vice-versa.
 *  - Copy a 1D image object to a scanline of a 2D slice of a 3D image object and vice-versa.
 *  - Copy a 1D image object to a scanline of a specific image index of a 1D or 2D image array object and vice-versa.
 *  - Copy a 2D image object to a 2D image object.
 *  - Copy a 2D image object to a 2D slice of a 3D image object and vice-versa.
 *  - Copy a 2D image object to a specific image index of a 2D image array object and vice-versa
 *  - Copy images from a 1D image array object to a 1D image array object.
 *  - Copy images from a 2D image array object to a 2D image array object.
 *  - Copy a 3D image object to a 3D image object.
 *
 *  \param command_queue refers to the command-queue in which the copy command will be queued. The OpenCL context
 * associated with command_queue, src_image and dst_image must be the same.
 *
 *  \param src_origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If image is a 2D image
 * object, src_origin[2] must be 0. If src_image is a 1D image object, src_origin[1] and src_origin[2] must be 0. If
 * src_image is a 1D image array object, src_origin[2] must be 0. If src_image is a 1D image array object, src_origin[1]
 * describes the image index in the 1D image array. If src_image is a 2D image array object, src_origin[2] describes
 * the image index in the 2D image array.
 *
 *  \param dst_origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If dst_image is a 2D image
 * object, dst_origin[2] must be 0. If dst_image is a 1D image or 1D image buffer object, dst_origin[1] and
 * dst_origin[2] must be 0. If dst_image is a 1D image array object, dst_origin[2] must be 0. If dst_image is a 1D image
 * array object, dst_origin[1] describes the image index in the 1D image array. If dst_image is a 2D image array
 * object, dst_origin[2] describes the image index in the 2D image array.
 *
 *  \param region defines the (width, height, depth) in pixels of the 1D, 2D or 3D rectangle, the (width, height) in
 * pixels of the 2D rectangle and the number of images of a 2D image array or the (width) in pixels of the 1D rectangle
 * and the number of images of a 1D image array. If src_image or dst_image is a 2D image object, region[2] must be 1. If
 * src_image or dst_image is a 1D image or 1D image buffer object, region[1] and region[2] must be 1. If src_image or
 * dst_image is a 1D image array object, region[2] must be 1. The values in region cannot be 0.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list and
 * command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function
 * returns.
 *
 *  \param event returns an event object that identifies this particular copy command and can be used to query or queue
 * a wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete.
 * clEnqueueBarrierWithWaitList can be used instead. If the event_wait_list and the event arguments are not NULL , the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  It is currently a requirement that the src_image and dst_image image memory objects for clEnqueueCopyImage must have
 * the exact same image format (i.e. the cl_image_format descriptor specified when src_image and dst_image are created
 * must match).
 *
 *  \return clEnqueueCopyImage returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue, src_image and dst_image are not the same or if
 * the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if src_image and dst_image are not valid image objects.
 *  - CL_IMAGE_FORMAT_MISMATCH if src_image and dst_image do not use the same image format.
 *  - CL_INVALID_VALUE if the 2D or 3D rectangular region specified by src_origin and src_origin + region refers to a
 * region outside src_image, or if the 2D or 3D rectangular region specified by dst_origin and dst_origin + region
 * refers to a region outside dst_image.
 *  - CL_INVALID_VALUE if values in src_origin, dst_origin and region do not follow rules described in the argument
 * description for src_origin, dst_origin and region.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_INVALID_IMAGE_SIZE if image dimensions (image width, height, specified or compute row and/or slice pitch) for
 * src_image or dst_image are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel order and data type) for src_image or dst_image are
 * not supported by device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with src_image
 * or dst_image.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *  - CL_INVALID_OPERATION if the device associated with command_queue does not support images (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_MEM_COPY_OVERLAP if src_image and dst_image are the same image object and the source and destination regions
 * overlap.
 */
cl_int VC4CL_FUNC(clEnqueueCopyImage)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_image,
    const size_t* src_origin, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueCopyImage, "cl_command_queue", command_queue, "cl_mem", src_image, "cl_mem",
        dst_image, "const size_t*", src_origin, "const size_t*", dst_origin, "const size_t*", region, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Image>(src_image))
    CHECK_BUFFER(toType<Image>(dst_image))
    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    return toType<Image>(src_image)->enqueueCopyInto(toType<CommandQueue>(command_queue), toType<Image>(dst_image),
        src_origin, dst_origin, region, num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 106+:
 *
 *  Enqueues a command to fill an image object with a specified color. The usage information which indicates whether the
 * memory object can be read or written by a kernel and/or the host and is given by the cl_mem_flags argument value
 *  specified when image is created is ignored by clEnqueueFillImage.
 *
 *  \param command_queue refers to the command-queue in which the fill command will be queued. The OpenCL context
 * associated with command_queue and image must be the same.
 *
 *  \param image is a valid image object.
 *
 *  \param fill_color is the fill color. The fill color is a four component RGBA floating-point color value if the image
 * channel data type is not an unnormalized signed and unsigned integer type, is a four component signed integer value
 * if the image channel data type is an unnormalized signed integer type and is a four component unsigned integer value
 * if the image channel data type is an unnormalized unsigned integer type. The fill color will be converted to the
 * appropriate image channel format and order associated with image as described in sections 6.12.14 and 8.3.
 *
 *  \param origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If image is a 2D image
 * object, origin[2] must be 0. If image is a 1D image or 1D image buffer object, origin[1] and origin[2] must be 0. If
 * image is a 1D image array object, origin[2] must be 0. If image is a 1D image array object, origin[1] describes the
 * image index in the 1D image array. If image is a 2D image array object, origin[2] describes the image index in the
 * 2D image array.
 *
 *  \param region defines the (width, height, depth) in pixels of the 1D, 2D or 3D rectangle, the (width, height) in
 * pixels of the 2D rectangle and the number of images of a 2D image array or the (width) in pixels of the 1D rectangle
 * and the number of images of a 1D image array. If image is a 2D image object, region[2] must be 1. If image is a 1D
 * image or 1D image buffer object, region[1] and region[2] must be 1. If image is a 1D image array object, region[2]
 * must be 1. The values in region cannot be 0.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a
 * wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete.
 * clEnqueueBarrierWithWaitList can be used instead. If the event_wait_list and the event arguments are not NULL , the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueFillImage returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue and image are not the same or if the context
 * associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if image is not a valid image object.
 *  - CL_INVALID_VALUE if fill_color is NULL .
 *  - CL_INVALID_VALUE if the region being filled as specified by origin and region is out of bounds or if ptr is a NULL
 * value.
 *  - CL_INVALID_VALUE if values in origin and region do not follow rules described in the argument description for
 * origin and region.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_INVALID_IMAGE_SIZE if image dimensions (image width, height, specified or compute row and/or slice pitch) for
 * image are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel order and data type) for image are not supported by
 * device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with image.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueFillImage)(cl_command_queue command_queue, cl_mem image, const void* fill_color,
    const size_t* origin, const size_t* region, cl_uint num_events_in_wait_list, const cl_event* event_wait_list,
    cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueFillImage, "cl_command_queue", command_queue, "cl_mem", image,
        "const void*", fill_color, "const size_t*", origin, "const size_t*", region, "cl_uint", num_events_in_wait_list,
        "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Image>(image))
    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    return toType<Image>(image)->enqueueFill(toType<CommandQueue>(command_queue), fill_color, origin, region,
        num_events_in_wait_list, event_wait_list, event);
}

/*!
 * OpenCL 1.2 specification, pages 108+:
 *
 *  Enqueues a command to copy an image object to a buffer object.
 *
 *  \param command_queue must be a valid command-queue. The OpenCL context associated with command_queue, src_image and
 * dst_buffer must be the same.
 *
 *  \param src_image is a valid image object.
 *
 *  \param dst_buffer is a valid buffer object.
 *
 *  \param src_origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If src_image is a 2D image
 * object, src_origin[2] must be 0. If src_image is a 1D image or 1D image buffer object, src_origin[1] and
 * src_origin[2] must be 0. If src_image is a 1D image array object, src_origin[2] must be 0. If src_image is a 1D image
 * array object, src_origin[1] describes the image index in the 1D image array. If src_image is a 2D image array
 * object, src_origin[2] describes the image index in the 2D image array.
 *
 *  \param region defines the (width, height, depth) in pixels of the 1D, 2D or 3D rectangle, the (width, height) in
 * pixels of the 2D rectangle and the number of images of a 2D image array or the (width) in pixels of the 1D rectangle
 * and the number of images of a 1D image array. If src_image is a 2D image object, region[2] must be 1. If src_image is
 * a 1D image or 1D image buffer object, region[1] and region[2] must be 1. If src_image is a 1D image array object,
 * region[2] must be 1. The values in region cannot be 0.
 *
 *  \param dst_offset refers to the offset where to begin copying data into dst_buffer. The size in bytes of the region
 * to be copied referred to as dst_cb is computed as width * height * depth * bytes/image element if src_image is a 3D
 * image object, is computed as width * height * bytes/image element if src_image is a 2D image, is computed as width *
 * height * arraysize * bytes/image element if src_image is a 2D image array object, is computed as width * bytes/image
 * element if src_image is a 1D image or 1D image buffer object and is computed as width * arraysize * bytes/image
 * element if src_image is a 1D image array object.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular copy command and can be used to query or queue
 * a wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete.
 * clEnqueueBarrierWithWaitList can be used instead. If the event_wait_list and the event arguments are not NULL , the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueCopyImageToBuffer returns CL_SUCCESS if the function is executed successfully. Otherwise, it
 * returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue, src_image and dst_buffer are not the same or if
 * the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if src_image is not a valid image object or dst_buffer is not a valid buffer object or if
 * src_image is a 1D image buffer object created from dst_buffer.
 *  - CL_INVALID_VALUE if the 1D, 2D or 3D rectangular region specified by src_origin and src_origin + region refers to
 * a region outside src_image, or if the region specified by dst_offset and dst_offset + dst_cb to a region outside
 * dst_buffer.
 *  - CL_INVALID_VALUE if values in src_origin and region do not follow rules described in the argument description for
 * src_origin and region.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if dst_buffer is a sub-buffer object and offset specified when the sub-buffer
 * object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_INVALID_IMAGE_SIZE if image dimensions (image width, height, specified or compute row and/or slice pitch) for
 * src_image are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel order and data type) for src_image are not supported
 * by device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with src_image
 * or dst_buffer.
 *  - CL_INVALID_OPERATION if the device associated with command_queue does not support images (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueCopyImageToBuffer)(cl_command_queue command_queue, cl_mem src_image, cl_mem dst_buffer,
    const size_t* src_origin, const size_t* region, size_t dst_offset, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueCopyImageToBuffer, "cl_command_queue", command_queue, "cl_mem", src_image,
        "cl_mem", dst_buffer, "const size_t*", src_origin, "const size_t*", region, "size_t", dst_offset, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Image>(src_image))
    CHECK_BUFFER(toType<Buffer>(dst_buffer))
    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    return toType<Image>(src_image)->enqueueCopyFromToBuffer(toType<CommandQueue>(command_queue),
        toType<Buffer>(dst_buffer), src_origin, region, dst_offset, false, num_events_in_wait_list, event_wait_list,
        event);
}

/*!
 * OpenCL 1.2 specification, pages 111+:
 *
 *  Enqueues a command to copy a buffer object to an image object.
 *
 *  \param command_queue must be a valid command-queue. The OpenCL context associated with command_queue, src_buffer and
 * dst_image must be the same.
 *
 *  \param src_buffer is a valid buffer object.
 *
 *  \param dst_image is a valid image object.
 *
 *  \param src_offset refers to the offset where to begin copying data from src_buffer.
 *
 *  \param dst_origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If dst_image is a 2D image
 * object, dst_origin[2] must be 0. If dst_image is a 1D image or 1D image buffer object, dst_origin[1] and
 * dst_origin[2] must be 0. If dst_image is a 1D image array object, dst_origin[2] must be 0. If dst_image is a 1D image
 * array object, dst_origin[1] describes the image index in the 1D image array. If dst_image is a 2D image array
 * object, dst_origin[2] describes the image index in the 2D image array.
 *
 *  \param region defines the (width, height, depth) in pixels of the 1D, 2D or 3D rectangle, the (width, height) in
 * pixels of the 2D rectangle and the number of images of a 2D image array or the (width) in pixels of the 1D rectangle
 * and the number of images of a 1D image array. If dst_image is a 2D image object, region[2] must be 1. If dst_image is
 * a 1D image or 1D image buffer object, region[1] and region[2] must be 1. If dst_image is a 1D image array object,
 * region[2] must be 1. The values in region cannot be 0.
 *
 *  The size in bytes of the region to be copied from src_buffer referred to as src_cb is computed as width * height *
 * depth * bytes/image element if dst_image is a 3D image object, is computed as width * height * bytes/image element if
 * dst_image is a 2D image, is computed as width * height * arraysize * bytes/image element if dst_image is a 2D image
 * array object, is computed as width * bytes/image element if dst_image is a 1D image or 1D image buffer object and is
 * computed as width * arraysize * bytes/image element if dst_image is a 1D image array object.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before this particular
 * command can be executed. If event_wait_list is NULL , then this particular command does not wait on any event to
 * complete. If event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list
 * of events pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events
 * specified in event_wait_list act as synchronization points. The context associated with events in event_wait_list
 * and command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the
 * function returns.
 *
 *  \param event returns an event object that identifies this particular copy command and can be used to query or queue
 * a wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete.
 * clEnqueueBarrierWithWaitList can be used instead. If the event_wait_list and the event arguments are not NULL , the
 * event argument should not refer to an element of the event_wait_list array.
 *
 *  \return clEnqueueCopyBufferToImage returns CL_SUCCESS if the function is executed successfully. Otherwise, it
 * returns one of the following errors:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if the context associated with command_queue, src_buffer and dst_image are not the same or if
 * the context associated with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if src_buffer is not a valid buffer object or dst_image is not a valid image object or if
 * dst_image is a 1D image buffer object created from src_buffer.
 *  - CL_INVALID_VALUE if the 1D, 2D or 3D rectangular region specified by dst_origin and dst_origin + region refer to a
 * region outside dst_image, or if the region specified by src_offset and src_offset + src_cb refer to a region outside
 * src_buffer.
 *  - CL_INVALID_VALUE if values in dst_origin and region do not follow rules described in the argument description for
 * dst_origin and region.
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_MISALIGNED_SUB_BUFFER_OFFSET if src_buffer is a sub-buffer object and offset specified when the sub-buffer
 * object is created is not aligned to CL_DEVICE_MEM_BASE_ADDR_ALIGN value for device associated with queue.
 *  - CL_INVALID_IMAGE_SIZE if image dimensions (image width, height, specified or compute row and/or slice pitch) for
 * dst_image are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel order and data type) for dst_image are not supported
 * by device associated with queue.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with
 * src_buffer or dst_image.
 *  - CL_INVALID_OPERATION if the device associated with command_queue does not support images (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clEnqueueCopyBufferToImage)(cl_command_queue command_queue, cl_mem src_buffer, cl_mem dst_image,
    size_t src_offset, const size_t* dst_origin, const size_t* region, cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list, cl_event* event)
{
    VC4CL_PRINT_API_CALL("cl_int", clEnqueueCopyBufferToImage, "cl_command_queue", command_queue, "cl_mem", src_buffer,
        "cl_mem", dst_image, "size_t", src_offset, "const size_t*", dst_origin, "const size_t*", region, "cl_uint",
        num_events_in_wait_list, "const cl_event*", event_wait_list, "cl_event*", event);
    CHECK_COMMAND_QUEUE(toType<CommandQueue>(command_queue))
    CHECK_BUFFER(toType<Buffer>(src_buffer))
    CHECK_BUFFER(toType<Image>(dst_image))
    CHECK_EVENT_WAIT_LIST(event_wait_list, num_events_in_wait_list)

    return toType<Image>(dst_image)->enqueueCopyFromToBuffer(toType<CommandQueue>(command_queue),
        toType<Buffer>(src_buffer), dst_origin, region, src_offset, true, num_events_in_wait_list, event_wait_list,
        event);
}

/*!
 * OpenCL 1.2 specification, pages 113+:
 *
 *  Enqueues a command to map a region in the image object given by image into the host address space and returns a
 * pointer to this mapped region.
 *
 *  \param command_queue must be a valid command-queue.
 *
 *  \param image is a valid image object. The OpenCL context associated with command_queue and image must be the same.
 *
 *  \param blocking_map indicates if the map operation is blocking or non-blocking.
 *  If blocking_map is CL_TRUE , clEnqueueMapImage does not return until the specified region in image is mapped into
 * the host address space and the application can access the contents of the mapped region using the pointer returned by
 * clEnqueueMapImage. If blocking_map is CL_FALSE i.e. map operation is non-blocking, the pointer to the mapped region
 * returned by clEnqueueMapImage cannot be used until the map command has completed. The event argument returns an
 * event object which can be used to query the execution status of the map command. When the map command is completed,
 * the application can access the contents of the mapped region using the pointer returned by clEnqueueMapImage.
 *
 *  \param map_flags is a bit-field and is described in table 5.5.
 *
 *  \param origin defines the (x, y, z) offset in pixels in the 1D, 2D or 3D image, the (x, y) offset and the image
 * index in the 2D image array or the (x) offset and the image index in the 1D image array. If image is a 2D image
 * object, origin[2] must be 0. If image is a 1D image or 1D image buffer object, origin[1] and origin[2] must be 0. If
 * image is a 1D image array object, origin[2] must be 0. If image is a 1D image array object, origin[1] describes the
 * image index in the 1D image array. If image is a 2D image array object, origin[2] describes the image index in the
 * 2D image array.
 *
 *  \param region defines the (width, height, depth) in pixels of the 1D, 2D or 3D rectangle, the (width, height) in
 * pixels of the 2D rectangle and the number of images of a 2D image array or the (width) in pixels of the 1D rectangle
 * and the number of images of a 1D image array. If image is a 2D image object, region[2] must be 1. If image is a 1D
 * image or 1D image buffer object, region[1] and region[2] must be 1. If image is a 1D image array object, region[2]
 * must be 1. The values in region cannot be 0.
 *
 *  \param image_row_pitch returns the scan-line pitch in bytes for the mapped region. This must be a non-NULL value.
 *
 *  \param image_slice_pitch returns the size in bytes of each 2D slice of a 3D image or the size of each 1D or 2D image
 * in a 1D or 2D image array for the mapped region. For a 1D and 2D image, zero is returned if this argument is not NULL
 * . For a 3D image, 1D and 2D image array, image_slice_pitch must be a non- NULL value.
 *
 *  \param event_wait_list and num_events_in_wait_list specify events that need to complete before clEnqueueMapImage can
 * be executed. If event_wait_list is NULL , then clEnqueueMapImage does not wait on any event to complete. If
 * event_wait_list is NULL , num_events_in_wait_list must be 0. If event_wait_list is not NULL , the list of events
 * pointed to by event_wait_list must be valid and num_events_in_wait_list must be greater than 0. The events specified
 * in event_wait_list act as synchronization points. The context associated with events in event_wait_list and
 * command_queue must be the same. The memory associated with event_wait_list can be reused or freed after the function
 * returns.
 *
 *  \param event returns an event object that identifies this particular command and can be used to query or queue a
 * wait for this particular command to complete. event can be NULL in which case it will not be possible for the
 * application to query the status of this command or queue a wait for this command to complete. If the event_wait_list
 * and the event arguments are not NULL , the event argument should not refer to an element of the event_wait_list
 * array.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clEnqueueMapImage will return a pointer to the mapped region. The errcode_ret is set to CL_SUCCESS. A NULL
 * pointer is returned otherwise with one of the following error values returned in errcode_ret:
 *  - CL_INVALID_COMMAND_QUEUE if command_queue is not a valid command-queue.
 *  - CL_INVALID_CONTEXT if context associated with command_queue and image are not the same or if context associated
 * with command_queue and events in event_wait_list are not the same.
 *  - CL_INVALID_MEM_OBJECT if image is not a valid image object.
 *  - CL_INVALID_VALUE if region being mapped given by (origin, origin+region) is out of bounds or if values specified
 * in map_flags are not valid.
 *  - CL_INVALID_VALUE if values in origin and region do not follow rules described in the argument description for
 * origin and region.
 *  - CL_INVALID_VALUE if image_row_pitch is NULL .
 *  - CL_INVALID_VALUE if image is a 3D image, 1D or 2D image array object and image_slice_pitch is NULL .
 *  - CL_INVALID_EVENT_WAIT_LIST if event_wait_list is NULL and num_events_in_wait_list > 0, or event_wait_list is not
 * NULL and num_events_in_wait_list is 0, or if event objects in event_wait_list are not valid events.
 *  - CL_INVALID_IMAGE_SIZE if image dimensions (image width, height, specified or compute row and/or slice pitch) for
 * image are not supported by device associated with queue.
 *  - CL_IMAGE_FORMAT_NOT_SUPPORTED if image format (image channel order and data type) for image are not supported by
 * device associated with queue.
 *  - CL_MAP_FAILURE if there is a failure to map the requested region into the host address space. This error cannot
 * occur for image objects created with CL_MEM_USE_HOST_PTR or CL_MEM_ALLOC_HOST_PTR.
 *  - CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST if the map operation is blocking and the execution status of any of
 * the events in event_wait_list is a negative integer value.
 *  - CL_MEM_OBJECT_ALLOCATION_FAILURE if there is a failure to allocate memory for data store associated with image.
 *  - CL_INVALID_OPERATION if the device associated with command_queue does not support images (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_INVALID_OPERATION if image has been created with CL_MEM_HOST_WRITE_ONLY or CL_MEM_HOST_NO_ACCESS and
 * CL_MAP_READ is set in map_flags or if image has been created with CL_MEM_HOST_READ_ONLY or CL_MEM_HOST_NO_ACCESS and
 * CL_MAP_WRITE or CL_MAP_WRITE_INVALIDATE_REGION is set in map_flags.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 *
 *  NOTE: The pointer returned maps a 1D, 2D or 3D region starting at origin and is at least region[0] pixels in size
 * for a 1D image, 1D image buffer or 1D image array, (image_row_pitch * region[1]) pixels in size for a 2D image or 2D
 * image array, and (image_slice_pitch * region[2]) pixels in size for a 3D image. The result of a memory access outside
 * this region is undefined. If the image object is created with CL_MEM_USE_HOST_PTR set in mem_flags, the following
 * will be true:
 *  - The host_ptr specified in clCreateImage is guaranteed to contain the latest bits in the region being mapped when
 * the clEnqueueMapImage command has completed.
 *  - The pointer value returned by clEnqueueMapImage will be derived from the host_ptr specified when the image object
 * is created.
 *
 *  Mapped image objects are unmapped using clEnqueueUnmapMemObject. This is described in section 5.4.2.
 */
void* VC4CL_FUNC(clEnqueueMapImage)(cl_command_queue command_queue, cl_mem image, cl_bool blocking_map,
    cl_map_flags map_flags, const size_t* origin, const size_t* region, size_t* image_row_pitch,
    size_t* image_slice_pitch, cl_uint num_events_in_wait_list, const cl_event* event_wait_list, cl_event* event,
    cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("void*", clEnqueueMapImage, "cl_command_queue", command_queue, "cl_mem", image, "cl_bool",
        blocking_map, "cl_map_flags", map_flags, "const size_t*", origin, "const size_t*", region, "size_t*",
        image_row_pitch, "size_t*", image_slice_pitch, "cl_uint", num_events_in_wait_list, "const cl_event*",
        event_wait_list, "cl_event*", event, "cl_int*", errcode_ret);
    CHECK_COMMAND_QUEUE_ERROR_CODE(toType<CommandQueue>(command_queue), errcode_ret, void*)
    CHECK_BUFFER_ERROR_CODE(toType<Image>(image), errcode_ret, void*)
    CHECK_EVENT_WAIT_LIST_ERROR_CODE(event_wait_list, num_events_in_wait_list, errcode_ret, void*)

    return toType<Image>(image)->enqueueMap(toType<CommandQueue>(command_queue), blocking_map, map_flags, origin,
        region, image_row_pitch, image_slice_pitch, num_events_in_wait_list, event_wait_list, event, errcode_ret);
}

/*!
 * OpenCL 1.2 specification, pages 116+:
 *
 *  To get information specific to an image object created with clCreateImage, use the following function:
 *
 *  \param image specifies the image object being queried.
 *
 *  \param param_name specifies the information to query. The list of supported param_name types and the information
 * returned in param_value by clGetImageInfo is described in table 5.9.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is
 * NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.9. param_value_size_ret returns the actual size in bytes of data being
 * queried by param_value. If param_value_size_ret is NULL , it is ignored.
 *
 *  \return clGetImageInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of the
 * following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return
 * type as described in table 5.9 and param_value is not NULL .
 *  - CL_INVALID_MEM_OBJECT if image is a not a valid image object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetImageInfo)(
    cl_mem image, cl_image_info param_name, size_t param_value_size, void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetImageInfo, "cl_mem", image, "cl_image_info", param_name, "size_t",
        param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
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
 *  \param normalized_coords determines if the image coordinates specified are normalized (if normalized_coords is
 * CL_TRUE ) or not (if normalized_coords is CL_FALSE ).
 *
 *  \param addressing_mode specifies how out-of-range image coordinates are handled when reading from an image. This can
 * be set to CL_ADDRESS_MIRRORED_REPEAT , CL_ADDRESS_REPEAT , CL_ADDRESS_CLAMP_TO_EDGE , CL_ADDRESS_CLAMP and
 * CL_ADDRESS_NONE .
 *
 *  \param filter_mode specifies the type of filter that must be applied when reading an image. This can be
 * CL_FILTER_NEAREST , or CL_FILTER_LINEAR .
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL , no error code is returned.
 *
 *  \return clCreateSampler returns a valid non-zero sampler object and errcode_ret is set to CL_SUCCESS if the sampler
 * object is created successfully. Otherwise, it returns a NULL value with one of the following error values returned in
 * errcode_ret:
 *  - CL_INVALID_CONTEXT if context is not a valid context .
 *  - CL_INVALID_VALUE if addressing_mode, filter_mode or normalized_coords or combination of these argument values are
 * not valid.
 *  - CL_INVALID_OPERATION if images are not supported by any device associated with context (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE ).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_sampler VC4CL_FUNC(clCreateSampler)(cl_context context, cl_bool normalized_coords,
    cl_addressing_mode addressing_mode, cl_filter_mode filter_mode, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_sampler", clCreateSampler, "cl_context", context, "cl_bool", normalized_coords,
        "cl_addressing_mode", addressing_mode, "cl_filter_mode", filter_mode, "cl_int*", errcode_ret);
    CHECK_CONTEXT_ERROR_CODE(toType<Context>(context), errcode_ret, cl_sampler)
#ifndef IMAGE_SUPPORT
    return returnError<cl_sampler>(
        CL_INVALID_OPERATION, errcode_ret, __FILE__, __LINE__, "Image support is not enabled!");
#endif

    if(normalized_coords != CL_TRUE && normalized_coords != CL_FALSE)
        return returnError<cl_sampler>(
            CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Invalid value for normalized coordinates!");

    // see OpenCL 1.2 specification, table 6.22
    if(normalized_coords != CL_TRUE && addressing_mode == CL_ADDRESS_MIRRORED_REPEAT)
        return returnError<cl_sampler>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
            "Mirrored repeat addressing mode can only be used with normalized coordinates!");
    if(normalized_coords != CL_TRUE && addressing_mode == CL_ADDRESS_REPEAT)
        return returnError<cl_sampler>(CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__,
            "Repeat addressing mode can only be used with normalized coordinates!");

    Sampler* sampler =
        newOpenCLObject<Sampler>(toType<Context>(context), normalized_coords == CL_TRUE, addressing_mode, filter_mode);
    CHECK_ALLOCATION_ERROR_CODE(sampler, errcode_ret, cl_sampler)
    RETURN_OBJECT(sampler->toBase(), errcode_ret)
}

#ifdef CL_VERSION_2_0
/*!
 * OpenCL 2.2 specification, pages 143+:
 *
 *  Creates a sampler object.
 *
 *  \param context must be a valid OpenCL context.
 *
 *  \param sampler_properties specifies a list of sampler property names and their corresponding values. Each sampler
 * property name is immediately followed by the corresponding desired value. The list is terminated with 0. The list of
 * supported properties is described in table 5.15. If a supported property and its value is not specified in
 * sampler_properties, its default value will be used. sampler_properties can be NULL in which case the default values
 * for supported sampler properties will be used.
 *
 *  \param errcode_ret will return an appropriate error code. If errcode_ret is NULL, no error code is returned.
 *
 *  \return clCreateSamplerWithProperties returns a valid non-zero sampler object and errcode_ret is set to CL_SUCCESS
 * if the sampler object is created successfully. Otherwise, it returns a NULL value with one of the following error
 * values returned in errcode_ret:
 *  - CL_INVALID_CONTEXT if _context_is not a valid context.
 *  - CL_INVALID_VALUE if the property name in sampler_properties is not a supported property name, if the value
 * specified for a supported property name is not valid, or if the same property name is specified more than once.
 *  - CL_INVALID_OPERATION if images are not supported by any device associated with context (i.e.
 * CL_DEVICE_IMAGE_SUPPORT specified in table 4.3 is CL_FALSE).
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_sampler VC4CL_FUNC(clCreateSamplerWithProperties)(
    cl_context context, const cl_sampler_properties* sampler_properties, cl_int* errcode_ret)
{
    VC4CL_PRINT_API_CALL("cl_sampler", clCreateSamplerWithProperties, "cl_context", context,
        "const cl_sampler_properties*", sampler_properties, "cl_int*", errcode_ret);
    cl_bool normalized_coords = CL_TRUE;
    cl_addressing_mode addressing_mode = CL_ADDRESS_CLAMP;
    cl_filter_mode filter_mode = CL_FILTER_NEAREST;

    if(sampler_properties != nullptr)
    {
        const cl_sampler_properties* prop = sampler_properties;
        while(*prop != 0)
        {
            if(*prop == CL_SAMPLER_NORMALIZED_COORDS)
            {
                ++prop;
                normalized_coords = static_cast<cl_bool>(*prop);
                ++prop;
            }
            else if(*prop == CL_SAMPLER_ADDRESSING_MODE)
            {
                ++prop;
                addressing_mode = static_cast<cl_addressing_mode>(*prop);
                ++prop;
            }
            else if(*prop == CL_SAMPLER_FILTER_MODE)
            {
                ++prop;
                filter_mode = static_cast<cl_filter_mode>(*prop);
                ++prop;
            }
            else
                return returnError<cl_sampler>(
                    CL_INVALID_VALUE, errcode_ret, __FILE__, __LINE__, "Unsupported sampler property");
        }
    }

    return VC4CL_FUNC(clCreateSampler)(context, normalized_coords, addressing_mode, filter_mode, errcode_ret);
}
#endif

/*!
 * OpenCL 1.2 specification, page 130:
 *
 *  Increments the sampler reference count. clCreateSampler performs an implicit retain.
 *
 *  \return clRetainSampler returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_SAMPLER if sampler is not a valid sampler object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clRetainSampler)(cl_sampler sampler)
{
    VC4CL_PRINT_API_CALL("cl_int", clRetainSampler, "cl_sampler", sampler);
    CHECK_SAMPLER(toType<Sampler>(sampler))
    return toType<Sampler>(sampler)->retain();
}

/*!
 * OpenCL 1.2 specification, page 130:
 *
 *  Decrements the sampler reference count. The sampler object is deleted after the reference count becomes zero and
 * commands queued for execution on a command-queue(s) that use sampler have finished.
 *
 *  \return clReleaseSampler returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_SAMPLER if sampler is not a valid sampler object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clReleaseSampler)(cl_sampler sampler)
{
    VC4CL_PRINT_API_CALL("cl_int", clReleaseSampler, "cl_sampler", sampler);
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
 *  \param param_name specifies the information to query. The list of supported param_name types and the information
 * returned in param_value by clGetSamplerInfo is described in table 5.12.
 *
 *  \param param_value is a pointer to memory where the appropriate result being queried is returned. If param_value is
 * NULL , it is ignored.
 *
 *  \param param_value_size is used to specify the size in bytes of memory pointed to by param_value. This size must be
 * >= size of return type as described in table 5.12.
 *
 *  \param param_value_size_ret returns the actual size in bytes of data copied to param_value. If param_value_size_ret
 * is NULL , it is ignored.
 *
 *  \return clGetSamplerInfo returns CL_SUCCESS if the function is executed successfully. Otherwise, it returns one of
 * the following errors:
 *  - CL_INVALID_VALUE if param_name is not valid, or if size in bytes specified by param_value_size is < size of return
 * type as described in table 5.12 and param_value is not NULL .
 *  - CL_INVALID_SAMPLER if sampler is a not a valid sampler object.
 *  - CL_OUT_OF_RESOURCES if there is a failure to allocate resources required by the OpenCL implementation on the
 * device.
 *  - CL_OUT_OF_HOST_MEMORY if there is a failure to allocate resources required by the OpenCL implementation on the
 * host.
 */
cl_int VC4CL_FUNC(clGetSamplerInfo)(cl_sampler sampler, cl_sampler_info param_name, size_t param_value_size,
    void* param_value, size_t* param_value_size_ret)
{
    VC4CL_PRINT_API_CALL("cl_int", clGetSamplerInfo, "cl_sampler", sampler, "cl_sampler_info", param_name, "size_t",
        param_value_size, "void*", param_value, "size_t*", param_value_size_ret);
    CHECK_SAMPLER(toType<Sampler>(sampler))
    return toType<Sampler>(sampler)->getInfo(param_name, param_value_size, param_value, param_value_size_ret);
}
