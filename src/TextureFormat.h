/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_TEXTURE_FORMAT
#define VC4CL_TEXTURE_FORMAT

#include <vector>
#include <utility>

namespace vc4cl
{
	class Image;

	using Coordinates2D = std::pair<std::size_t, std::size_t>;

	struct TextureAccessor
	{
	public:

		virtual ~TextureAccessor();

		virtual void* calculatePixelOffset(void* basePointer, const std::size_t pixelCoordinates[3]) const = 0;

		virtual std::size_t readSinglePixel(const std::size_t pixelCoordinates[3], void* output, const std::size_t outputSize) const;
		virtual bool writeSinglePixel(const std::size_t pixelCoordinates[3], const void* input, const std::size_t inputSize) const;

		virtual void readPixelData(const std::size_t pixelCoordinates[3], const std::size_t pixelRegion[3], void* output, std::size_t outputRowPitch, std::size_t outputSlicePitch) const;
		virtual void writePixelData(const std::size_t pixelCoordinates[3], const std::size_t pixelRegion[3], void* source, std::size_t sourceRowPitch, std::size_t sourceSlicePitch) const;

		static bool copyPixelData(const TextureAccessor& source, TextureAccessor& destination, const std::size_t sourceCoordinates[3], const std::size_t destCoordinates[3], const std::size_t pixelRegion[3]);

		static TextureAccessor* createTextureAccessor(Image& image);

	protected:
		Image& image;

		TextureAccessor(Image& image);
	};

	struct TFormatAccessor : public TextureAccessor
	{
	public:
		TFormatAccessor(Image& image);
		virtual ~TFormatAccessor();

		void* calculatePixelOffset(void* basePointer, const std::size_t pixelCoordinates[3]) const override;
	};

	struct LTFormatAccessor : public TextureAccessor
	{
	public:
		LTFormatAccessor(Image& image);
		virtual ~LTFormatAccessor();

		void* calculatePixelOffset(void* basePointer, const std::size_t pixelCoordinates[3]) const override;
	};

	enum class TileType
	{
		//64 Byte micro-tile, contains pixels in standard raster order
		MICROTILE,
		//1 KB tile, contains micro-tiles in standard raster order
		SUB_TILE,
		//4KB tile, contains 4 sub-tiles, order depends on index of 4k-tile
		TILE_4K
	};

	template<TileType T>
	struct Tile
	{
		static std::size_t BYTE_SIZE;

		/*
		 * Calculates the indices for the given coordinates in units of this type
		 */
		static Coordinates2D calculateIndices(const Image& image, const std::size_t pixelCoordinates[2]);
		/*
		 * Calculates the offsets for the given coordinates in units of the sub-type contained in this type
		 */
		static Coordinates2D calculateOffsets(const Image& image, const std::size_t pixelCoordinates[2]);
		/*
		 * Returns the width and height of this tile-size in units of the sub-type contained in this type
		 */
		static Coordinates2D getTileSize(const Image& image);
	};

	template<>
	struct Tile<TileType::MICROTILE>
	{
		static constexpr std::size_t BYTE_SIZE{64};

		static Coordinates2D calculateIndices(const Image& image, const std::size_t pixelCoordinates[2])
		{
			const Coordinates2D tileSize = getTileSize(image);
			return std::make_pair(pixelCoordinates[0] / tileSize.first, pixelCoordinates[1] / tileSize.second);
		}

		static Coordinates2D calculateOffsets(const Image& image, const std::size_t pixelCoordinates[2])
		{
			const Coordinates2D tileSize = getTileSize(image);
			return std::make_pair(pixelCoordinates[0] % tileSize.first, pixelCoordinates[1] % tileSize.second);
		}

		static Coordinates2D getTileSize(const Image& image);
	};

	template<>
	struct Tile<TileType::SUB_TILE>
	{
		static constexpr std::size_t BYTE_SIZE{1024};

		static Coordinates2D calculateIndices(const Image& image, const std::size_t pixelCoordinates[2])
		{
			const Coordinates2D tileSize = getTileSize(image);
			return std::make_pair(pixelCoordinates[0] / tileSize.first, pixelCoordinates[1] / tileSize.second);
		}

		static Coordinates2D calculateOffsets(const Image& image, const std::size_t pixelCoordinates[2])
		{
			const Coordinates2D tileSize = getTileSize(image);
			return std::make_pair(pixelCoordinates[0] % tileSize.first, pixelCoordinates[1] % tileSize.second);
		}

		static Coordinates2D getTileSize(const Image& image)
		{
			return std::make_pair(4, 4);
		}
	};

	template<>
	struct Tile<TileType::TILE_4K>
	{
		static constexpr std::size_t BYTE_SIZE{4 * 1024};

		static Coordinates2D calculateIndices(const Image& image, const std::size_t pixelCoordinates[2]);
		static Coordinates2D calculateOffsets(const Image& image, const std::size_t pixelCoordinates[2]);

		static Coordinates2D getTileSize(const Image& image)
		{
			return std::make_pair(2, 2);
		}
	};

	using Microtile = Tile<TileType::MICROTILE>;
	using Subtile = Tile<TileType::SUB_TILE>;
	using Tile4K = Tile<TileType::TILE_4K>;


} /* namespace vc4cl */

#endif /* VC4CL_TEXTURE_FORMAT */
