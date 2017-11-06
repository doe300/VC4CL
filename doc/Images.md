# Images

### OpenCL supported image types

List of all OpenCL 1.2 supported channel types and their allowed channel order types (OpenCL 1.2 specification, tables 5.6 and 5.7).

|                     | Component width | CL_R    | CL_Rx   | CL_A    | CL_INTENSITY | CL_LUMINANCE | CL_RG   | CL_RGx  | CL_RA   | CL_RGB  | CL_RGBx | CL_RGBA      | CL_ARGB | CL_BGRA |
|---------------------|-----------------|---------|---------|---------|--------------|--------------|---------|---------|---------|---------|---------|--------------|---------|---------|
| CL_SNORM_INT8       | 1 Byte          | &check; | &check; | &check; | &check;      | &check;      | &check; | &check; | &check; |         |         | &check;      | &check; | &check; |
| CL_SNORM_INT16      | 2 Bytes         | &check; | &check; | &check; | &check;      | &check;      | &check; | &check; | &check; |         |         | &check;      |         |         |
| CL_UNORM_INT8       | 1 Byte          | &check; | &check; | &check; | &check;      | &check;      | &check; | &check; | &check; |         |         | &check; (\*) | &check; | &check; |
| CL_UNORM_INT16      | 2 Bytes         | &check; | &check; | &check; | &check;      | &check;      | &check; | &check; | &check; |         |         | &check; (\*) |         |         |
| CL_UNORM_SHORT_565  | 2 Bytes         |         |         |         |              |              |         |         |         | &check; | &check; |              |         |         |
| CL_UNORM_SHORT_555  | 2 Bytes         |         |         |         |              |              |         |         |         | &check; | &check; |              |         |         |
| CL_UNORM_INT_101010 | 4 Bytes         | &check; | &check; | &check; |              |              | &check; | &check; | &check; | &check; | &check; | &check;      |         |         |
| CL_SIGNED_INT8      | 1 Byte          | &check; | &check; | &check; |              |              | &check; | &check; | &check; |         |         | &check; (\*) | &check; | &check; |
| CL_SIGNED_INT16     | 2 Bytes         | &check; | &check; | &check; |              |              | &check; | &check; | &check; |         |         | &check; (\*) |         |         |
| CL_SIGNED_INT32     | 4 Bytes         | &check; | &check; | &check; |              |              | &check; | &check; | &check; |         |         | &check; (\*) |         |         |
| CL_UNSIGNED_INT8    | 1 Byte          | &check; | &check; | &check; |              |              | &check; | &check; | &check; |         |         | &check; (\*) | &check; | &check; |
| CL_UNSIGNED_INT16   | 2 Bytes         | &check; | &check; | &check; |              |              | &check; | &check; | &check; |         |         | &check; (\*) |         |         |
| CL_UNSIGNED_INT32   | 4 Bytes         | &check; | &check; | &check; |              |              | &check; | &check; | &check; |         |         | &check; (\*) |         |         |
| CL_HALF_FLOAT       | 2 Bytes         | &check; | &check; | &check; | &check;      | &check;      | &check; | &check; | &check; |         |         | &check; (\*) |         |         |
| CL_FLOAT            | 4 Bytes         | &check; | &check; | &check; | &check;      | &check;      | &check; | &check; | &check; |         |         | &check; (\*) |         |         |

Entries marked (\*) are required to be supported (OpenCL 1.2 specification, table 5.8).

| Channel type | Components | Storage    | Resulting components |
|--------------|------------|------------|----------------------|
| CL_R         | 1          | r          | (r, 0.0, 0.0, 1.0)   |
| CL_Rx        | 1          | r          | (r, 0.0, 0.0, 1.0)   |
| CL_A         | 1          | a          | (0.0, 0.0, 0.0, a)   |
| CL_RG        | 2          | r, g       | (r, g, 0.0, 1.0)     |
| CL_RGx       | 2          | r, g       | (r, g, 0.0, 1.0)     |
| CL_RA        | 2          | r, a       | (r, 0.0, 0.0, a)     |
| CL_RGB       | 3          | r, g, b    | (r, g, b, 1.0)       |
| CL_RGBx      | 3          | r, g, b    | (r, g, b, 1.0)       |
| CL_RGBA      | 4          | r, g, b, a | (r, g, b, a)         |
| CL_BGRA      | 4          | b, r, g, a | (r, g, b, a)         |
| CL_ARGB      | 4          | a, r, g, b | (r, g, b, a)         |
| CL_INTENSITY | 1          | I          | (I, I, I, I)         |
| CL_LUMINANCE | 1          | L          | (L, L, L, 1.0)       |


#### Intel extension for packed YUV images
[This extension](https://www.khronos.org/registry/OpenCL/extensions/intel/cl_intel_packed_yuv.txt) enables support for packed YUV image channel orders.
At least the channel-order YUYV could be supported by a built-in VideoCore IV texture-type. The only channel-type supported for YUYV images is CL_UNORM_INT8.
The border-color for YUV-images is (0.0f, 0.0f, 0.0f, 1.0f), same as e.g. CL_RGB. The pixel-data read is mapped to the components ( V, Y, U, 1.0 ).

### VideoCore IV texture types

List of all texture types supported by the VideoCore IV hardware (Broadcom specification, table 18)

| Type          | Components | Storage    | Component width | Read components |
|---------------|------------|------------|-----------------|-----------------|
| RGBA8888      | 4          | r, g, b, a | 1 Byte          | (r, g, b, a)    |
| RGBX8888      | 3          | r, g, b    | 1 Byte          | (r, g, b, 1.0)  |
| RGBA4444      | 4          | r, g, b, a | 4 Bits          | (r, g, b, a)    |
| RGBA5551      | 4          | r, g, b, a | 5/1 Bits        | (r, g, b, a)    |
| RGB565        | 3          | r, g, b    | 5/6 Bits        | (r, g, b, 1.0)  |
| LUMINANCE     | 1          | L          | 1 Byte          | (L, L, L, 1.0)  |
| ALPHA         | 1          | a          | 1 Byte          | (0, 0, 0, a)    |
| LUMALPHA      | 2          | L, a       | 1 Byte          | (L, L, L, a)    |
| ETC1          | ?          | ?          | ?               | ?               |
| S16F          | 1          | s          | 2 Byte          | (s)             |
| S8            | 1          | s          | 1 Byte          | (s)             |
| S16           | 1          | s          | 2 Byte          | (s)             |
| BW1           | 1          | bw         | 1 Bit           | ?               |
| A4            | 1          | a          | 4 Bits          | ?               |
| A1            | 1          | a          | 1 Bit           | ?               |
| RGBA64        | 4          | r, g, b, a | 2 Bytes         | (r, g, b, a)    |
| RGBA32R (\*)  | 4          | r, g, b, a | 1 Byte          | (r, g, b, a)    |
| YUYV422R (\*) | 4          | y, u, y, v | 1 Byte          | (r, g, b, a) or (y, u, y', v) ? |

Entries marked (\*) are in "raster format" (rectangular grid of pixels), e.g. from videos/images (formats like bmp, gif), other formats are in T-format (see below)

**YUYV422** is used in video compressions (e.g. in PAL for TV, hence the raster format). For the memory layout of  [see here](https://wiki.multimedia.cx/index.php?title=PIX_FMT_YUYV422) and [here](https://en.wikipedia.org/wiki/YUV).

### Type mapping

Draft of mapping OpenCL image and component-type to VideoCore IV texture types.

|                     | CL_R    | CL_Rx   | CL_A    | CL_INTENSITY | CL_LUMINANCE | CL_RG   | CL_RGx  | CL_RA   | CL_RGB    | CL_RGBx   | CL_RGBA           | CL_ARGB      | CL_BGRA      | CL_YUYV_INTEL |
|---------------------|---------|---------|---------|--------------|--------------|---------|---------|---------|-----------|-----------|-------------------|--------------|--------------|---------------|
| CL_SNORM_INT8       | S8      | S8      | ALPHA?  | LUMINANCE?   | LUMINANCE?   |         |         |         | &cross;   | &cross;   | RGBA8888(R)?      | RGBA8888(R)? | RGBA8888(R)? | &cross;       |
| CL_SNORM_INT16      | S16     | S16     | S16     | S16          | S16          |         |         |         | &cross;   | &cross;   |                   | &cross;      | &cross;      | &cross;       |
| CL_UNORM_INT8       | S8      | S8      | ALPHA?  | LUMINANCE?   | LUMINANCE    |         |         |         | &cross;   | &cross;   | RGBA8888(R) (\*)  | RGBA8888(R)? | RGBA8888(R)? | YUYV422R ?    |
| CL_UNORM_INT16      | S16     | S16     | S16     | S16          | S16          |         |         |         | &cross;   | &cross;   |             (\*)  | &cross;      | &cross;      | &cross;       |
| CL_UNORM_SHORT_565  | &cross; | &cross; | &cross; | &cross;      | &cross;      | &cross; | &cross; | &cross; | RGB565    | RGB565    | &cross;           | &cross;      | &cross;      | &cross;       |
| CL_UNORM_SHORT_555  | &cross; | &cross; | &cross; | &cross;      | &cross;      | &cross; | &cross; | &cross; | RGBA5551? | RGBA5551? | &cross;           | &cross;      | &cross;      | &cross;       |
| CL_UNORM_INT_101010 |         |         |         | &cross;      | &cross;      |         |         |         |           |           |                   | &cross;      | &cross;      | &cross;       |
| CL_SIGNED_INT8      | S8      | S8      | S8      | &cross;      | &cross;      |         |         |         | &cross;   | &cross;   | RGBA8888(R)? (\*) | RGBA8888(R)? | RGBA8888(R)? | &cross;       |
| CL_SIGNED_INT16     | S16     | S16     | S16     | &cross;      | &cross;      |         |         |         | &cross;   | &cross;   |              (\*) | &cross;      | &cross;      | &cross;       |
| CL_SIGNED_INT32     |         |         |         | &cross;      | &cross;      |         |         |         | &cross;   | &cross;   |              (\*) | &cross;      | &cross;      | &cross;       |
| CL_UNSIGNED_INT8    | S8      | S8      | S8      | &cross;      | &cross;      |         |         |         | &cross;   | &cross;   | RGBA8888(R)  (\*) | RGBA8888(R)  | RGBA8888(R)  | &cross;       |
| CL_UNSIGNED_INT16   | S16     | S16     | S16     | &cross;      | &cross;      |         |         |         | &cross;   | &cross;   |              (\*) | &cross;      | &cross;      | &cross;       |
| CL_UNSIGNED_INT32   |         |         |         | &cross;      | &cross;      |         |         |         | &cross;   | &cross;   |              (\*) | &cross;      | &cross;      | &cross;       |
| CL_HALF_FLOAT       | S16F    | S16F    | S16F    | S16F         | S16F         |         |         |         | &cross;   | &cross;   | RGBA64       (\*) | &cross;      | &cross;      | &cross;       |
| CL_FLOAT            |         |         |         |              |              |         |         |         | &cross;   | &cross;   |              (\*) | &cross;      | &cross;      | &cross;       |

Entries marked (\*) are required to be supported (OpenCL 1.2 specification, table 5.8).

Texture formats directly matching the OpenCL image format (e.g. CL_RGBA, CL_UNORM_INT8 and RGBA8888(R)) need only to be unpacked into their components. For any other format (e.g. CL_HALF_FLOAT, CL_LUMINANCE and S16F) the constant components need to be set after unpacking.

## Texture formats

VideoCore IV supports T and LT-format (as well as raster-format for RGBA32R and YUYV422R)

A **micro-tile** is a "rectangular image block with a fixed size of [...] 64 bytes". Micro-tiles are stored "in simple raster order". (Broadcom specification, page 105)

| Pixel size | micro-tile size |
|------------|-----------------|
| 8 Bytes    | 2 x 4           |
| 4 Bytes    | 4 x 4           |
| 2 Bytes    | 8 x 4           |
| 1 Byte     | 8 x 8           |
| 4 Bits     | 16 x 8          |
| 1 Bit      | 32 x 16         |

Addressing offset within micro-tile for 4-Byte pixels:

|   |   |   |   |
|---|---|---|---|
| C | D | E | F |
| 8 | 9 | A | B |
| 4 | 5 | 6 | 7 |
| 0 | 1 | 2 | 3 |

## T-format
"T-format is based around 4 Kbyte tiles of 2D image data, formed from 1 Kbyte sub-tiles of data. As an example, for 32bpp pixel mode, 1 Kbyte equates to a block of 16 × 16 image pixels. 
A 4 Kbyte tile therefore contains 32 × 32 pixels."
The data needs to be padded to multiples of 4KB in width and height. 
(Broadcom specification, pages 105+)

Micro-tiles are stored in "normal" raster order within the sub-tiles (4 x 4 micro-tiles per sub-tile), resulting in the same addressing offsets as the example for within a micro-tile with 32-bits pixels. 
Sub-tiles are stored in circular order within a 4k tiles. The addressing offset depends on the row of the 4k tile. For even rows they are ordered bottom-left, up-left, up-right to bottom-right. For odd rows up-right, bottom-right, bottom-left, up-left:

| even | row |
|------|-----|
| 1    | 2   |
| 0    | 3   |

| odd | row |
|-----|-----|
| 2   | 0   |
| 3   | 1   |

The order of the 4-k tiles themselves is left-to-right for odd rows and right-to-left for even rows:

|   |   |   |   |   |
|---|---|---|---|---|
| k | l | m | n | o |
| j | i | h | g | f |
| a | b | c | d | e |
| 9 | 8 | 7 | 6 | 5 |
| 0 | 1 | 2 | 3 | 4 |

## LT-format
"Linear-tile format is typically used for small textures that are smaller than a full T-format 4K tile, to avoid wasting memory in padding the image out to be a multiple of tiles in size. 
This format is also micro-tile based but simply stores micro-tiles in a standard raster order." (Broadcom specification, page 107)

The LT-format is automatically selected for smaller size. (Broadcom specification, page 39)
"The hardware assumes a level is in T-format unless either the width or height for the level is less than one T-format tile. In this case use the hardware assumes the level is stored in LT-format." (Broadcom specification, page 40)

## Raster format
The texture-types RGBA32R and YUYV422R are in raster format, meaning all pixels in a row are in consecutive memory, followed by the next row and the next and so on. This allows for easier access host-side, e.g. pixels are a simple 1D/2D/3D array in memory, no custom order required, allows for memcpy instruction.

Though not officially supported as texture-format by the VideoCore IV architecture, the general 32-bit TMU read could be used to read raster-images of arbitrary size. For this to work, the address would need to be calculated from the image width and height. Instead of the general 32-bit TMU read, VPM could be used too (is more complicated to set up and requires the hardware-mutex to be locked, but way faster).

Another trick to read 4 components with 32-bit each (e.g. for CL_RGBA with CL_FLOAT) would be, to set the image-width to 4x the original width, read 4 pixels and combine into one vector. This only works for original image-widths up to 512 pixels (since 4 * 512 = 2048). Similar for 2 components with 32-bit each or 4 components with 16-bit each, could imitate an image twice the size, read 2 pixels and rearrange to correct components. Interpolations are not supported by this kind of image-access.