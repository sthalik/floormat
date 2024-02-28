#include "impl.hpp"
#include "compat/assert.hpp"
#include <Corrade/Containers/Array.h>
#include <Magnum/Math/Vector4.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

Trade::ImageData2D loader_impl::make_error_texture(Vector2ui size)
{
    fm_assert(size.product() != 0);
    constexpr auto magenta = Vector4ub{255, 0, 255, 255};
    auto array = Array<char>{NoInit, 4uz * size.product()};
    auto data = array.data(), end = data + array.size();
    while (data != end)
    {
        *(Vector4ub*)data = magenta;
        data += 4;
    }
    auto img = Trade::ImageData2D{PixelFormat::RGBA8Unorm, Vector2i(size), move(array)};
    return img;
}

} // namespace floormat::loader_detail
