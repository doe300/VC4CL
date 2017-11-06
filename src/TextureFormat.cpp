/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TextureFormat.h"
#include "Image.h"


using namespace vc4cl;

static std::size_t getRowPitchInBytes(const Image& image)
{
	if(image.imageRowPitch != 0)
		return image.imageRowPitch;
	return image.imageWidth * image.calculateElementSize();
}

static std::size_t getSlicePitchInBytes(const Image& image)
{
	if(image.imageSlicePitch != 0)
		return image.imageSlicePitch;
	return getRowPitchInBytes(image) * image.imageHeight;
}

TextureAccessor::TextureAccessor(Image& image) : image(image)
{

}

TextureAccessor::~TextureAccessor()
{
}

std::size_t TextureAccessor::readSinglePixel(const std::array<std::size_t, 3>& pixelCoordinates, void* output, const std::size_t outputSize) const
{
	const void* ptr = calculatePixelOffset(image.deviceBuffer->hostPointer, pixelCoordinates);
	const std::size_t pixelWidth = image.calculateElementSize();

	if(outputSize < pixelWidth)
		return 0;

	memcpy(output, ptr, pixelWidth);
	return pixelWidth;
}

bool TextureAccessor::writeSinglePixel(const std::array<std::size_t, 3>& pixelCoordinates, const void* input, const std::size_t inputSize) const
{
	void* ptr = calculatePixelOffset(image.deviceBuffer->hostPointer, pixelCoordinates);
	const std::size_t pixelWidth = image.calculateElementSize();

	if(inputSize < pixelWidth)
		return false;

	memcpy(ptr, input, pixelWidth);
	return true;
}

void TextureAccessor::readPixelData(const std::array<std::size_t, 3>& pixelCoordinates, const std::array<std::size_t, 3>& pixelRegion, void* output, std::size_t outputRowPitch, std::size_t outputSlicePitch) const
{
	const std::size_t pixelWidth = image.calculateElementSize();
	std::array<std::size_t, 3> inputCoords;
	for(std::size_t z = 0; z < pixelRegion[2]; ++z)
	{
		inputCoords[2] = pixelCoordinates[2] + z;
		for(std::size_t y = 0; y < pixelRegion[1]; ++y)
		{
			inputCoords[1] = pixelCoordinates[1] + y;
			for(std::size_t x = 0; x < pixelRegion[0]; ++x)
			{
				inputCoords[0] = pixelCoordinates[0] + x;
				void* outPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(output) + z * outputSlicePitch + y * outputRowPitch + x * pixelWidth);
				readSinglePixel(inputCoords, outPtr, pixelWidth);
			}
		}
	}
}

void TextureAccessor::writePixelData(const std::array<std::size_t, 3>& pixelCoordinates, const std::array<std::size_t, 3>& pixelRegion, void* source, std::size_t sourceRowPitch, std::size_t sourceSlicePitch) const
{
	const std::size_t pixelWidth = image.calculateElementSize();
	std::array<std::size_t, 3> outputCoords;
	for(std::size_t z = 0; z < pixelRegion[2]; ++z)
	{
		outputCoords[2] = pixelCoordinates[2] + z;
		for(std::size_t y = 0; y < pixelRegion[1]; ++y)
		{
			outputCoords[1] = pixelCoordinates[1] + y;
			for(std::size_t x = 0; x < pixelRegion[0]; ++x)
			{
				outputCoords[0] = pixelCoordinates[0] + x;
				void* inPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(source) + z * sourceSlicePitch + y * sourceRowPitch + x * pixelWidth);
				writeSinglePixel(outputCoords, inPtr, pixelWidth);
			}
		}
	}
}

void TextureAccessor::fillPixelData(const std::array<std::size_t, 3>& pixelCoordinates, const std::array<std::size_t, 3>& pixelRegion, void* fillColor) const
{
	const std::size_t pixelWidth = image.calculateElementSize();
	std::array<std::size_t, 3> outputCoords;
	for(std::size_t z = 0; z < pixelRegion[2]; ++z)
	{
		outputCoords[2] = pixelCoordinates[2] + z;
		for(std::size_t y = 0; y < pixelRegion[1]; ++y)
		{
			outputCoords[1] = pixelCoordinates[1] + y;
			for(std::size_t x = 0; x < pixelRegion[0]; ++x)
			{
				outputCoords[0] = pixelCoordinates[0] + x;
				writeSinglePixel(outputCoords, fillColor, pixelWidth);
			}
		}
	}
}

bool TextureAccessor::copyPixelData(const TextureAccessor& source, TextureAccessor& destination, const std::array<std::size_t, 3>& sourceCoordinates, const std::array<std::size_t, 3>& destCoordinates, const std::array<std::size_t, 3>& pixelRegion)
{
	//maximum pixel size is 4 components * 64-bit
	char buffer[64];
	std::array<std::size_t, 3> inputCoords;
	std::array<std::size_t, 3> outputCoords;
	for(std::size_t z = 0; z < pixelRegion[2]; ++z)
	{
		inputCoords[2] = sourceCoordinates[2] + z;
		outputCoords[2] = destCoordinates[2] + z;
		for(std::size_t y = 0; y < pixelRegion[1]; ++y)
		{
			inputCoords[1] = sourceCoordinates[1] + y;
			outputCoords[1] = destCoordinates[1] + y;
			for(std::size_t x = 0; x < pixelRegion[0]; ++x)
			{
				inputCoords[0] = sourceCoordinates[0] + x;
				outputCoords[0] = destCoordinates[0] + x;

				const std::size_t numBytes = source.readSinglePixel(inputCoords, buffer, sizeof(buffer));
				if(!destination.writeSinglePixel(outputCoords, buffer, numBytes))
					return false;
			}
		}
	}
	return true;
}

TextureAccessor* TextureAccessor::createTextureAccessor(Image& image)
{
	if(image.textureType.isRasterFormat)
		return new RasterFormatAccessor(image);
	/*
	 * "The hardware assumes a level is in T-format unless either the width or height for the level is less than one T-format tile.
	 * In this case use the hardware assumes the level is stored in LT-format." (Broadcom specification, page 40)
	 */
	//image-size >= 4KB -> T-format
	//image-size < 4KB -> LT-format
	if(getSlicePitchInBytes(image) >= Tile4K::BYTE_SIZE)
	{
		return new TFormatAccessor(image);
	}
	return new LTFormatAccessor(image);
}

//source: https://stackoverflow.com/questions/3407012/c-rounding-up-to-the-nearest-multiple-of-a-number
static size_t roundUp(size_t numToRound, size_t multiple)
{
    size_t remainder = numToRound % multiple;
    if (remainder == 0)
        return numToRound;

    return numToRound + multiple - remainder;
}

TFormatAccessor::TFormatAccessor(Image& image) : TextureAccessor(image)
{

}

TFormatAccessor::~TFormatAccessor()
{

}

int TFormatAccessor::checkAndApplyPitches(size_t srcRowPitch, size_t srcSlicePitch) const
{
	//The data needs to padded to 64Byte in both dimensions (Broadcom specification, pages 105+)
	//The 64Byte calculate from sqrt(4096), or 4 * 4 * 4 (4 micro-tiles with 4 pixels per 4 byte, see Broadcom specification, figure 15)
	if(srcRowPitch != 0)
	{
		if(srcRowPitch % 64 != 0)
			return CL_INVALID_VALUE;
		image.imageRowPitch = srcRowPitch;
	}
	else
		image.imageRowPitch = roundUp(image.imageWidth * image.calculateElementSize(), 64);
	if(srcSlicePitch != 0)
	{
		if(srcSlicePitch % (64 * image.imageRowPitch) != 0)
			return CL_INVALID_VALUE;
		image.imageSlicePitch = srcSlicePitch;
	}
	else
		image.imageSlicePitch = roundUp(image.imageHeight, 64) * image.imageRowPitch;
	return CL_SUCCESS;
}

void* TFormatAccessor::calculatePixelOffset(void* basePointer, const std::array<std::size_t, 3>& pixelCoordinates) const
{
	//TODO

	//for even 4K-tile rows, the sub-tiles are ordered down-left, up-left, up-right, down-right
	//for uneven 4K-tile rows, the sub-tiles are ordered up-right, down-right, down-left, up-left
	//for even 4K-tile rows, they are sorted left-to-right, for uneven rows right-to-left
}

LTFormatAccessor::LTFormatAccessor(Image& image) : TextureAccessor(image)
{

}

LTFormatAccessor::~LTFormatAccessor()
{

}

int LTFormatAccessor::checkAndApplyPitches(size_t srcRowPitch, size_t srcSlicePitch) const
{
	//This format stores micro-tiles in raster order, so pitches need to be multiple of micro-tile size
	const Coordinates2D mtSize = Microtile::getTileSize(image);
	if(srcRowPitch != 0)
	{
		if(srcRowPitch % (mtSize.x * image.calculateElementSize()) != 0)
			return CL_INVALID_VALUE;
		image.imageRowPitch = srcRowPitch;
	}
	else
		image.imageRowPitch = roundUp(image.imageWidth, mtSize.x) * image.calculateElementSize();
	if(srcSlicePitch != 0)
	{
		if(srcSlicePitch % (mtSize.y * image.imageRowPitch) != 0)
			return CL_INVALID_VALUE;
		image.imageSlicePitch = srcSlicePitch;
	}
	else
		image.imageSlicePitch = roundUp(image.imageHeight, mtSize.y) * image.imageRowPitch;

	return CL_SUCCESS;
}

void* LTFormatAccessor::calculatePixelOffset(void* basePointer, const std::array<std::size_t, 3>& pixelCoordinates) const
{
	/*
	 * "Linear-tile format is typically used for small textures that are smaller than a full T-format 4K tile,
	 * to avoid wasting memory in padding the image out to be a multiple of tiles in size.
	 * This format is also micro-tile based but simply stores micro-tiles in a standard raster order."
	 * - Broadcom specification, page 107
	 */
	const Coordinates2D mtSize = Microtile::getTileSize(image);
	const Coordinates2D mtIndex = Microtile::calculateIndices(image, pixelCoordinates.data());
	const Coordinates2D mtOffset = Microtile::calculateOffsets(image, pixelCoordinates.data());

	const uintptr_t offsetSlice = pixelCoordinates[3] * getSlicePitchInBytes(image);

	/*
	 * Indices of micro-tiles:
	 *
	 * bbbbbbx
	 * aaaaaaaaaaaaaaa
	 * aaaaaaaaaaaaaaa
	 */
	const uintptr_t offsetA = mtIndex.toByteOffset(getRowPitchInBytes(image)/ (mtSize.x * image.calculateElementSize()), Microtile::BYTE_SIZE);

	/*
	 * Indices within a micro-tile:
	 *
	 * ddx
	 * cccc
	 * cccc
	 */
	const uintptr_t offsetB = mtOffset.toByteOffset(mtSize.x, image.calculateElementSize());
	return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(basePointer) + offsetA + offsetB + offsetSlice);
}

RasterFormatAccessor::RasterFormatAccessor(Image& image) : TextureAccessor(image)
{

}

RasterFormatAccessor::~RasterFormatAccessor()
{

}

cl_int RasterFormatAccessor::checkAndApplyPitches(size_t srcRowPitch, size_t srcSlicePitch) const
{
	//nothing to do, any pitch is okay
	image.imageRowPitch = srcRowPitch == 0 ? image.imageWidth * image.calculateElementSize() : srcRowPitch;
	image.imageSlicePitch = srcSlicePitch == 0 ? image.imageHeight * image.imageRowPitch : srcSlicePitch;
	return CL_SUCCESS;
}

void* RasterFormatAccessor::calculatePixelOffset(void* basePointer, const std::array<std::size_t, 3>& pixelCoordinates) const
{
	//TODO image arrays (1D, 2D)
	const size_t widthOffset = pixelCoordinates[0] * image.calculateElementSize();
	const size_t heightOffset = image.imageRowPitch * pixelCoordinates[1];
	const size_t depthOffset = image.imageSlicePitch * pixelCoordinates[2];

	return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(basePointer) + widthOffset + heightOffset + depthOffset);
}

void RasterFormatAccessor::readPixelData(const std::array<std::size_t, 3>& pixelCoordinates, const std::array<std::size_t, 3>& pixelRegion, void* output, std::size_t outputRowPitch, std::size_t outputSlicePitch) const
{
	const std::size_t pixelWidth = image.calculateElementSize();
	std::array<std::size_t, 3> inputCoords;
	inputCoords[0] = pixelCoordinates[0];
	for(std::size_t z = 0; z < pixelRegion[2]; ++z)
	{
		inputCoords[2] = pixelCoordinates[2] + z;
		for(std::size_t y = 0; y < pixelRegion[1]; ++y)
		{
			inputCoords[1] = pixelCoordinates[1] + y;
			void* outPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(output) + z * outputSlicePitch + y * outputRowPitch);
			const void* inPtr = calculatePixelOffset(image.deviceBuffer->hostPointer, inputCoords);
			memcpy(outPtr, inPtr, pixelRegion[0] * pixelWidth);
		}
	}
}

void RasterFormatAccessor::writePixelData(const std::array<std::size_t, 3>& pixelCoordinates, const std::array<std::size_t, 3>& pixelRegion, void* source, std::size_t sourceRowPitch, std::size_t sourceSlicePitch) const
{
	const std::size_t pixelWidth = image.calculateElementSize();
	std::array<std::size_t, 3> outputCoords;
	outputCoords[0] = pixelCoordinates[0];
	for(std::size_t z = 0; z < pixelRegion[2]; ++z)
	{
		outputCoords[2] = pixelCoordinates[2] + z;
		for(std::size_t y = 0; y < pixelRegion[1]; ++y)
		{
			outputCoords[1] = pixelCoordinates[1] + y;
			void* inPtr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(source) + z * sourceSlicePitch + y * sourceRowPitch);
			void* outPtr = calculatePixelOffset(image.deviceBuffer->hostPointer, outputCoords);
			memcpy(outPtr, inPtr, pixelRegion[0] * pixelWidth);
		}
	}
}

Coordinates2D Microtile::getTileSize(const Image& image)
{
	//see Broadcom specification, page 105
	switch(image.calculateElementSize())
	{
		case 8:
			return Coordinates2D(2, 4);
		case 4:
			return Coordinates2D(4, 4);
		case 2:
			return Coordinates2D(8, 4);
		case 1:
			return Coordinates2D(8, 8);
	}
	throw std::invalid_argument("Invalid image and channel types to calculate pixel size");
}

