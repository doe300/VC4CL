/*
 * Author: doe300
 *
 * See the file "LICENSE" for the full license governing this code.
 */

#ifndef VC4CL_TEXTURE_FORMAT
#define VC4CL_TEXTURE_FORMAT

#include <array>
#include <utility>
#include <vector>

namespace vc4cl
{
    class Image;

    struct Coordinates2D
    {
        const std::size_t x;
        const std::size_t y;

        constexpr Coordinates2D(std::size_t x, std::size_t y) : x(x), y(y) {}

        /*
         * NOTE: This function is only correct for standard raster order!
         */
        constexpr std::size_t toByteOffset(std::size_t widthInElements, std::size_t elementSize) const
        {
            return (y * widthInElements + x) * elementSize;
        }
    };

    struct TextureAccessor
    {
    public:
        virtual ~TextureAccessor() noexcept = default;

        virtual int checkAndApplyPitches(size_t srcRowPitch, size_t srcSlicePitch) const = 0;
        virtual void* calculatePixelOffset(
            void* basePointer, const std::array<std::size_t, 3>& pixelCoordinates) const = 0;

        virtual std::size_t readSinglePixel(
            const std::array<std::size_t, 3>& pixelCoordinates, void* output, std::size_t outputSize) const;
        virtual bool writeSinglePixel(
            const std::array<std::size_t, 3>& pixelCoordinates, const void* input, std::size_t inputSize) const;

        virtual void readPixelData(const std::array<std::size_t, 3>& pixelCoordinates,
            const std::array<std::size_t, 3>& pixelRegion, void* output, std::size_t outputRowPitch,
            std::size_t outputSlicePitch) const;
        virtual void writePixelData(const std::array<std::size_t, 3>& pixelCoordinates,
            const std::array<std::size_t, 3>& pixelRegion, void* source, std::size_t sourceRowPitch,
            std::size_t sourceSlicePitch) const;

        virtual void fillPixelData(const std::array<std::size_t, 3>& pixelCoordinates,
            const std::array<std::size_t, 3>& pixelRegion, void* fillColor) const;

        static bool copyPixelData(const TextureAccessor& source, TextureAccessor& destination,
            const std::array<std::size_t, 3>& sourceCoordinates, const std::array<std::size_t, 3>& destCoordinates,
            const std::array<std::size_t, 3>& pixelRegion);

        static TextureAccessor* createTextureAccessor(Image& image);

    protected:
        Image& image;

        explicit TextureAccessor(Image& image);
    };

    struct TFormatAccessor final : public TextureAccessor
    {
    public:
        explicit TFormatAccessor(Image& image);
        ~TFormatAccessor() noexcept override = default;

        int checkAndApplyPitches(size_t srcRowPitch, size_t srcSlicePitch) const override;
        void* calculatePixelOffset(void* basePointer, const std::array<std::size_t, 3>& pixelCoordinates) const override
            __attribute__((pure));
    };

    struct LTFormatAccessor final : public TextureAccessor
    {
    public:
        explicit LTFormatAccessor(Image& image);
        ~LTFormatAccessor() noexcept override = default;

        int checkAndApplyPitches(size_t srcRowPitch, size_t srcSlicePitch) const override;
        void* calculatePixelOffset(
            void* basePointer, const std::array<std::size_t, 3>& pixelCoordinates) const override;
    };

    struct RasterFormatAccessor final : public TextureAccessor
    {
    public:
        explicit RasterFormatAccessor(Image& image);
        ~RasterFormatAccessor() noexcept override = default;

        int checkAndApplyPitches(size_t srcRowPitch, size_t srcSlicePitch) const override;
        void* calculatePixelOffset(void* basePointer, const std::array<std::size_t, 3>& pixelCoordinates) const override
            __attribute__((pure));

        // These function are overridden to provide a better performance by copying a scan-line at once and not every
        // single pixel
        void readPixelData(const std::array<std::size_t, 3>& pixelCoordinates,
            const std::array<std::size_t, 3>& pixelRegion, void* output, std::size_t outputRowPitch,
            std::size_t outputSlicePitch) const override;
        void writePixelData(const std::array<std::size_t, 3>& pixelCoordinates,
            const std::array<std::size_t, 3>& pixelRegion, void* source, std::size_t sourceRowPitch,
            std::size_t sourceSlicePitch) const override;
    };

    enum class TileType
    {
        // 64 Byte micro-tile, contains pixels in standard raster order
        MICROTILE,
        // 1 KB tile, contains micro-tiles in standard raster order
        SUB_TILE,
        // 4KB tile, contains 4 sub-tiles, order depends on index of 4k-tile
        TILE_4K
    };

    template <TileType T>
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

    template <>
    struct Tile<TileType::MICROTILE>
    {
        static constexpr std::size_t BYTE_SIZE{64};

        static Coordinates2D calculateIndices(const Image& image, const std::size_t pixelCoordinates[2])
        {
            const Coordinates2D tileSize = getTileSize(image);
            return Coordinates2D(pixelCoordinates[0] / tileSize.x, pixelCoordinates[1] / tileSize.y);
        }

        static Coordinates2D calculateOffsets(const Image& image, const std::size_t pixelCoordinates[2])
        {
            const Coordinates2D tileSize = getTileSize(image);
            return Coordinates2D(pixelCoordinates[0] % tileSize.x, pixelCoordinates[1] % tileSize.y);
        }

        static Coordinates2D getTileSize(const Image& image);
    };

    template <>
    struct Tile<TileType::SUB_TILE>
    {
        static constexpr std::size_t BYTE_SIZE{1024};

        static Coordinates2D calculateIndices(const Image& image, const std::size_t pixelCoordinates[2])
        {
            const Coordinates2D tileSize = getTileSize(image);
            return Coordinates2D(pixelCoordinates[0] / tileSize.x, pixelCoordinates[1] / tileSize.y);
        }

        static Coordinates2D calculateOffsets(const Image& image, const std::size_t pixelCoordinates[2])
        {
            const Coordinates2D tileSize = getTileSize(image);
            return Coordinates2D(pixelCoordinates[0] % tileSize.x, pixelCoordinates[1] % tileSize.y);
        }

        static Coordinates2D getTileSize(const Image& image)
        {
            // TODO correct?? For the calculation of indices/offsets? Don't we need the size in pixels?
            return Coordinates2D(4, 4);
        }
    };

    template <>
    struct Tile<TileType::TILE_4K>
    {
        static constexpr std::size_t BYTE_SIZE{4 * 1024};

        static Coordinates2D calculateIndices(const Image& image, const std::size_t pixelCoordinates[2])
        {
            const Coordinates2D tileSize = getTileSize(image);
            return Coordinates2D(pixelCoordinates[0] / tileSize.x, pixelCoordinates[1] / tileSize.y);
        }

        static Coordinates2D calculateOffsets(const Image& image, const std::size_t pixelCoordinates[2])
        {
            const Coordinates2D tileSize = getTileSize(image);
            return Coordinates2D(pixelCoordinates[0] % tileSize.x, pixelCoordinates[1] % tileSize.y);
        }

        static Coordinates2D getTileSize(const Image& image)
        {
            return Coordinates2D(2, 2);
        }
    };

    using Microtile = Tile<TileType::MICROTILE>;
    using Subtile = Tile<TileType::SUB_TILE>;
    using Tile4K = Tile<TileType::TILE_4K>;

} /* namespace vc4cl */

#endif /* VC4CL_TEXTURE_FORMAT */
