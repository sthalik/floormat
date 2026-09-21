#include "stb-zlib.hpp"

// STBI_NO_PNG alone makes stb_image.h:535 define STBI_NO_ZLIB and drop the inflate code.
#define STBI_SUPPORT_ZLIB
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_JPEG
#define STBI_NO_PNG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM
#include <stb_image.h>

namespace floormat {

bool inflate_zlib(char* dst, uint32_t dst_size, const char* src, uint32_t src_size)
{
    return stbi_zlib_decode_buffer(dst, (int)dst_size, src, (int)src_size) == (int)dst_size;
}

} // namespace floormat
