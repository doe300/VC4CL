/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#include "TextureFormat.h"
#include "Image.h"


using namespace vc4cl;

TextureAccessor::TextureAccessor(Image& image) : image(image)
{

}

TextureAccessor::~TextureAccessor()
{
}

std::size_t TextureAccessor::readSinglePixel(const std::size_t pixelCoordinates[3], void* output, const std::size_t outputSize) const
{
	const void* ptr = calculatePixelOffset(image.deviceBuffer->hostPointer, pixelCoordinates);
	const std::size_t pixelWidth = image.calculateElementSize();

	if(outputSize < pixelWidth)
		return 0;

	memcpy(output, ptr, pixelWidth);
	return pixelWidth;
}

bool TextureAccessor::writeSinglePixel(const std::size_t pixelCoordinates[3], const void* input, const std::size_t inputSize) const
{
	void* ptr = calculatePixelOffset(image.deviceBuffer->hostPointer, pixelCoordinates);
	const std::size_t pixelWidth = image.calculateElementSize();

	if(inputSize < pixelWidth)
		return false;

	memcpy(ptr, input, pixelWidth);
	return true;
}

void TextureAccessor::readPixelData(const std::size_t pixelCoordinates[3], const std::size_t pixelRegion[3], void* output, std::size_t outputRowPitch, std::size_t outputSlicePitch) const
{
	const std::size_t pixelWidth = image.calculateElementSize();
	std::size_t inputCoords[3];
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

void TextureAccessor::writePixelData(const std::size_t pixelCoordinates[3], const std::size_t pixelRegion[3], void* source, std::size_t sourceRowPitch, std::size_t sourceSlicePitch) const
{
	const std::size_t pixelWidth = image.calculateElementSize();
	std::size_t outputCoords[3];
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


bool TextureAccessor::copyPixelData(const TextureAccessor& source, TextureAccessor& destination, const std::size_t sourceCoordinates[3], const std::size_t destCoordinates[3], const std::size_t pixelRegion[3])
{
	//maximum pixel size is 4 components * 64-bit
	char buffer[64];
	std::size_t inputCoords[3];
	std::size_t outputCoords[3];
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
	/*
	 * "The hardware assumes a level is in T-format unless either the width or height for the level is less than one T-format tile.
	 * In this case use the hardware assumes the level is stored in LT-format." (Broadcom specification, page 40)
	 */
	//image-size >= 4KB -> T-format
	//image-size < 4KB -> LT-format
	if(image.imageSlicePitch >= Tile4K::BYTE_SIZE ||
			image.calculateElementSize() * image.imageRowPitch * image.imageHeight >= Tile4K::BYTE_SIZE ||
			image.calculateElementSize() * image.imageWidth * image.imageHeight >= Tile4K::BYTE_SIZE)
	{
		//TODO The data needs to be padded to multiples of 4KB in width and height. (Broadcom specification, pages 105+)
		return new TFormatAccessor(image);
	}
	return new LTFormatAccessor(image);
}

TFormatAccessor::TFormatAccessor(Image& image) : TextureAccessor(image)
{

}

TFormatAccessor::~TFormatAccessor()
{

}

void* TFormatAccessor::calculatePixelOffset(void* basePointer, const std::size_t pixelCoordinates[3]) const
{
	//TODO
}

LTFormatAccessor::LTFormatAccessor(Image& image) : TextureAccessor(image)
{

}

LTFormatAccessor::~LTFormatAccessor()
{

}

void* LTFormatAccessor::calculatePixelOffset(void* basePointer, const std::size_t pixelCoordinates[3]) const
{
	/*
	 * "Linear-tile format is typically used for small textures that are smaller than a full T-format 4K tile,
	 * to avoid wasting memory in padding the image out to be a multiple of tiles in size.
	 * This format is also micro-tile based but simply stores micro-tiles in a standard raster order."
	 * - Broadcom specification, page 107
	 */
	const Coordinates2D mtSize = Microtile::getTileSize(image);
	const Coordinates2D mtIndex = Microtile::calculateIndices(image, pixelCoordinates);
	const Coordinates2D mtOffset = Microtile::calculateOffsets(image, pixelCoordinates);

	const uintptr_t offsetSlice = pixelCoordinates[3] * image.imageSlicePitch;

	/*
	 * Indices of micro-tiles:
	 *
	 * bbbbbbx
	 * aaaaaaaaaaaaaaa
	 * aaaaaaaaaaaaaaa
	 */
	const uintptr_t offsetA = mtIndex.second * image.imageRowPitch;
	const uintptr_t offsetB = mtIndex.first * Microtile::BYTE_SIZE;

	/*
	 * Indices within a micro-tile:
	 *
	 * ddx
	 * cccc
	 * cccc
	 */
	const uintptr_t offsetC = mtOffset.second * mtSize.second * image.calculateElementSize();
	const uintptr_t offsetD = mtOffset.first * image.calculateElementSize();
	return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(basePointer) + offsetA + offsetB + offsetC + offsetD + offsetSlice);
}

Coordinates2D Microtile::getTileSize(const Image& image)
{
	//see Broadcom specification, page 105
	switch(image.calculateElementSize())
	{
		case 8:
			return std::make_pair(2, 4);
		case 4:
			return std::make_pair(4, 4);
		case 2:
			return std::make_pair(8, 4);
		case 1:
			return std::make_pair(8, 8);
	}
	throw std::invalid_argument("Invalid image and channel types to calculate pixel size");
}
