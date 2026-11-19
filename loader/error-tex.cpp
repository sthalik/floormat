#include "impl.hpp"
#include "compat/assert.hpp"
#include <cstring>
#include <cr/Array.h>
#include <mg/Vector4.h>
#include <mg/PixelFormat.h>
#include <mg/PixelStorage.h>
#include <mg/ImageData.h>

namespace floormat::detail::loader {

Trade::ImageData2D loader_impl::make_error_texture(Vector2ui size, PixelFormat format)
{
    constexpr auto magenta = Vector4ub{255, 0, 255, 255};
    return make_error_texture(size, format, magenta);
}

Trade::ImageData2D loader_impl::make_error_texture(Vector2ui size, PixelFormat format, Vector4ub color)
{
    fm_assert(size.product() != 0);
    fm_assert(format == PixelFormat::RGB8Unorm || format == PixelFormat::RGBA8Unorm);
    const usize pixel_size = pixelFormatSize(format);
    auto array = Array<char>{NoInit, pixel_size * size.product()};
    for (usize i = 0; i < array.size(); i += pixel_size)
        std::memcpy(array.data() + i, color.data(), pixel_size);
    auto img = Trade::ImageData2D{PixelStorage{}.setAlignment(1), format, Vector2i(size), move(array)};
    return img;
}

} // namespace floormat::detail::loader
