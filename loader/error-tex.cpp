#include "impl.hpp"
#include <Magnum/Math/Vector4.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Trade/ImageData.h>

namespace floormat::loader_detail {

Trade::ImageData2D loader_impl::make_error_texture()
{
    static const Vector4ub data[] = { {255, 0, 255, 255} }; // magenta
    return Trade::ImageData2D{PixelFormat::RGBA8Unorm, {1, 1}, {},
                              Containers::arrayView(data, 1), {}, {}};
}

} // namespace floormat::loader_detail
