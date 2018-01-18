/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TestImage.h"

#include "src/Image.h"
#include "src/icd_loader.h"

#include <algorithm>
#include <numeric>

using namespace vc4cl;

TestImage::TestImage() : context(nullptr), queue(nullptr)
{
#ifdef IMAGE_SUPPORT
	TEST_ADD(TestImage::testImageLimits);
	TEST_ADD(TestImage::testImageFormats);
	TEST_ADD(TestImage::testImageTypes);
	TEST_ADD(TestImage::testHostsideTFormat);
	TEST_ADD(TestImage::testHostsideLTFormat);
	TEST_ADD(TestImage::testHostsideRasterFormat);
#if HAS_COMPILER
	TEST_ADD(TestImage::testDeviceTFormatRead);
	TEST_ADD(TestImage::testDeviceLTFormatRead);
	TEST_ADD(TestImage::testDeviceRasterFormatRead);
	TEST_ADD(TestImage::testDeviceImageWrite);
#endif
#endif
}

void TestImage::testImageLimits()
{
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	std::array<char, 128> buffer;
	size_t valueSize = 0;
	cl_int status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE_SUPPORT, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT_EQUALS(static_cast<cl_bool>(CL_TRUE), *reinterpret_cast<const cl_bool*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_MAX_READ_IMAGE_ARGS, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(8 <= *reinterpret_cast<const cl_uint*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_MAX_WRITE_IMAGE_ARGS, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(1 <= *reinterpret_cast<const cl_uint*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE2D_MAX_WIDTH, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(2048 <= *reinterpret_cast<const size_t*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE2D_MAX_HEIGHT, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(2048 <= *reinterpret_cast<const size_t*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE3D_MAX_WIDTH, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	//TEST_ASSERT(0 <= *reinterpret_cast<const size_t*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE3D_MAX_HEIGHT, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	//TEST_ASSERT(0 <= *reinterpret_cast<const size_t*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE3D_MAX_DEPTH, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	//TEST_ASSERT(0 <= *reinterpret_cast<const size_t*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE_MAX_BUFFER_SIZE, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(2048 <= *reinterpret_cast<const size_t*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_IMAGE_MAX_ARRAY_SIZE, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(256 <= *reinterpret_cast<const size_t*>(buffer.data()));

	status = VC4CL_FUNC(clGetDeviceInfo)(device_id, CL_DEVICE_MAX_SAMPLERS, buffer.size(), buffer.data(), &valueSize);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(8 <= *reinterpret_cast<const cl_uint*>(buffer.data()));
}

//see OpenCL 1.2 specification, page 357
static std::vector<cl_image_format> requiredFormats = {
		cl_image_format{CL_RGBA, CL_UNORM_INT8},
		cl_image_format{CL_RGBA, CL_UNORM_INT16},
		cl_image_format{CL_RGBA, CL_SIGNED_INT8},
		cl_image_format{CL_RGBA, CL_SIGNED_INT16},
		cl_image_format{CL_RGBA, CL_SIGNED_INT32},
		cl_image_format{CL_RGBA, CL_UNSIGNED_INT8},
		cl_image_format{CL_RGBA, CL_UNSIGNED_INT16},
		cl_image_format{CL_RGBA, CL_UNSIGNED_INT32},
		cl_image_format{CL_RGBA, CL_HALF_FLOAT},
		cl_image_format{CL_RGBA, CL_FLOAT}
};

void TestImage::testImageFormats()
{
	std::array<cl_image_format, 128> formats;
	cl_uint numFormats = 0;
	cl_int status = VC4CL_FUNC(clGetSupportedImageFormats)(context, 0, CL_MEM_OBJECT_IMAGE2D, formats.size(), formats.data(), &numFormats);

	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT_MSG(numFormats < formats.size(), "The number of image-formats returned is the size of the buffer, consider increasing the buffer-size!");

	std::vector<cl_image_format> remainingRequiredFormats(requiredFormats);
	for(cl_uint i = 0; i < numFormats; ++i)
	{
		const cl_image_format format = formats.at(i);
		auto it = std::find_if(remainingRequiredFormats.begin(), remainingRequiredFormats.end(), [format](const cl_image_format f) -> bool
		{
			return f.image_channel_data_type == format.image_channel_data_type && f.image_channel_order == format.image_channel_order;
		});
		if(it != remainingRequiredFormats.end())
			remainingRequiredFormats.erase(it);
	}

	for(cl_image_format format : remainingRequiredFormats)
	{
		TEST_FAIL("Required image-format is not supported");
	}
}

void TestImage::testImageTypes()
{
	cl_image_format format{CL_RGBA, CL_UNSIGNED_INT8};
	cl_int status = CL_SUCCESS;
	cl_mem image = VC4CL_FUNC(clCreateImage2D)(context, 0, &format, 2048, 2048, 2048 /* too small */, nullptr, &status);
	TEST_ASSERT_EQUALS(nullptr, image);
	TEST_ASSERT_EQUALS(CL_INVALID_IMAGE_DESCRIPTOR, status);

	image = VC4CL_FUNC(clCreateImage2D)(context, 0, &format, 2048, 2048, 0, nullptr, &status);
	TEST_ASSERT(image != nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT_EQUALS(static_cast<size_t>(4), toType<Image>(image)->calculateElementSize());

	status = VC4CL_FUNC(clReleaseMemObject(image));
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);

	//TODO more tests with pitches/sizes/etc. out of bounds
}

void TestImage::testHostsideTFormat()
{
	cl_image_format format{CL_RGBA, CL_HALF_FLOAT};
	cl_int status = CL_SUCCESS;
	cl_mem image = VC4CL_FUNC(clCreateImage2D)(context, 0, &format, 2048, 2048, 0, nullptr, &status);
	TEST_ASSERT(image != nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(dynamic_cast<TFormatAccessor*>(toType<Image>(image)->accessor.get()) != nullptr);

	std::array<unsigned, 256> buffer;
	std::iota(buffer.begin(), buffer.end(), 0);

	const std::array<size_t, 3> origin = {0, 0, 0};
	const std::array<size_t, 3> region = {128, 0, 0};

	//errors on invalid parameters
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, origin.data(), region.data(), 7, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_INVALID_VALUE, status);
	std::array<size_t, 3> wrongOrigin = {0, 0, 3};
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, wrongOrigin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_INVALID_VALUE, status);
	wrongOrigin[0] = 2048;
	wrongOrigin[2] = 0;
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, wrongOrigin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_INVALID_VALUE, status);

	//fill first row
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, origin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	//fill some other row
	const std::array<size_t, 3> offsetOrigin = {1024, 1024, 0};
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, offsetOrigin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);

	std::array<unsigned, buffer.size()> tmp;
	//check first area filled with correct values
	status = VC4CL_FUNC(clEnqueueReadImage)(queue, image, CL_TRUE, origin.data(), region.data(), 0, 0, tmp.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(buffer == tmp);
	//check second area filled with correct values
	status = VC4CL_FUNC(clEnqueueReadImage)(queue, image, CL_TRUE, offsetOrigin.data(), region.data(), 0, 0, tmp.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(buffer == tmp);

	status = VC4CL_FUNC(clReleaseMemObject(image));
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
}

void TestImage::testHostsideLTFormat()
{

}

void TestImage::testHostsideRasterFormat()
{
	cl_image_format format{CL_RGBA, CL_UNORM_INT8};
	cl_int status = CL_SUCCESS;
	cl_mem image = VC4CL_FUNC(clCreateImage2D)(context, 0, &format, 2048, 2048, 0, nullptr, &status);
	TEST_ASSERT(image != nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(toType<Image>(image) != nullptr && toType<Image>(image)->textureType.isRasterFormat);
	TEST_ASSERT(dynamic_cast<RasterFormatAccessor*>(toType<Image>(image)->accessor.get()) != nullptr);

	std::array<unsigned, 256> buffer;
	std::iota(buffer.begin(), buffer.end(), 0);

	const std::array<size_t, 3> origin = {0, 0, 0};
	const std::array<size_t, 3> region = {256, 0, 0};

	//errors on invalid parameters
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, origin.data(), region.data(), 7, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_INVALID_VALUE, status);
	std::array<size_t, 3> wrongOrigin = {0, 0, 3};
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, wrongOrigin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_INVALID_VALUE, status);
	wrongOrigin[0] = 2048;
	wrongOrigin[2] = 0;
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, wrongOrigin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_INVALID_VALUE, status);

	//fill first row
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, origin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	//fill some other row
	const std::array<size_t, 3> offsetOrigin = {1024, 1024, 0};
	status = VC4CL_FUNC(clEnqueueWriteImage)(queue, image, CL_TRUE, offsetOrigin.data(), region.data(), 0, 0, buffer.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);

	std::array<unsigned, buffer.size()> tmp;
	//check first area filled with correct values
	status = VC4CL_FUNC(clEnqueueReadImage)(queue, image, CL_TRUE, origin.data(), region.data(), 0, 0, tmp.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(buffer == tmp);
	//check second area filled with correct values
	status = VC4CL_FUNC(clEnqueueReadImage)(queue, image, CL_TRUE, offsetOrigin.data(), region.data(), 0, 0, tmp.data(), 0, nullptr, nullptr);
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
	TEST_ASSERT(buffer == tmp);

	status = VC4CL_FUNC(clReleaseMemObject(image));
	TEST_ASSERT_EQUALS(CL_SUCCESS, status);
}

void TestImage::testDeviceTFormatRead()
{
	//TODO read a few random pixels and return values, check host-side for match
	//also trigger conversions on read
}

void TestImage::testDeviceLTFormatRead()
{

}

void TestImage::testDeviceRasterFormatRead()
{

}

void TestImage::testDeviceImageWrite()
{

}

bool TestImage::setup()
{
	cl_int errcode = CL_SUCCESS;
	cl_device_id device_id = Platform::getVC4CLPlatform().VideoCoreIVGPU.toBase();
	context = VC4CL_FUNC(clCreateContext)(nullptr, 1, &device_id, nullptr, nullptr, &errcode);
	queue = VC4CL_FUNC(clCreateCommandQueue)(context, device_id, 0, &errcode);
	return errcode == CL_SUCCESS && context != nullptr && queue != nullptr;
}
void TestImage::tear_down()
{
	VC4CL_FUNC(clReleaseCommandQueue)(queue);
	VC4CL_FUNC(clReleaseContext)(context);
}
