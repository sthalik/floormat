
#include "tile-atlas.hpp"
#include "compat/assert.hpp"
#include "tile-image.hpp"
#include <limits>
#include <Corrade/Containers/StringStl.h>
#include <Magnum/Math/Color.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

tile_atlas::tile_atlas(StringView name, const ImageView2D& image, Vector2ub tile_count) :
    texcoords_{make_texcoords_array(Vector2ui(image.size()), tile_count)},
    name_{name}, size_{image.size()}, dims_{tile_count}
{
    constexpr auto variant_max = std::numeric_limits<variant_t>::max();
    fm_assert(num_tiles() <= variant_max);
    fm_assert(dims_[0] > 0 && dims_[1] > 0);
    fm_assert(size_ % Vector2ui{tile_count} == Vector2ui());
    tex_.setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Linear)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(1)
        .setBorderColor(Color4{1, 0, 0, 1})
        .setStorage(1, GL::textureFormat(image.format()), image.size())
        .setSubImage(0, {}, image);
}

std::array<Vector2, 4> tile_atlas::texcoords_for_id(std::size_t i) const
{
    fm_assert(i < num_tiles());
    return texcoords_[i];
}

auto tile_atlas::make_texcoords(Vector2ui pixel_size, Vector2ub tile_count, std::size_t i) -> texcoords
{
    const auto sz = pixel_size/Vector2ui{tile_count};
    const Vector2ui id = { std::uint32_t(i % tile_count[0]), std::uint32_t(i / tile_count[0]) };
    const Vector2 p0(id * sz), p1(sz);
    const auto x0 = p0.x()+.5f, x1 = p1.x()-1, y0 = p0.y()+.5f, y1 = p1.y()-1;
    return {{
        { (x0+x1) / pixel_size[0], 1 - (y0+y1) / pixel_size[1]  }, // bottom right
        { (x0+x1) / pixel_size[0], 1 -      y0 / pixel_size[1]  }, // top right
        {      x0 / pixel_size[0], 1 - (y0+y1) / pixel_size[1]  }, // bottom left
        {      x0 / pixel_size[0], 1 -     y0  / pixel_size[1]  }, // top left
    }};
}

auto tile_atlas::make_texcoords_array(Vector2ui pixel_size, Vector2ub tile_count) -> std::unique_ptr<const texcoords[]>
{
    const std::size_t N = Vector2ui{tile_count}.product();
    auto ptr = std::make_unique<std::array<Vector2, 4>[]>(N);
    for (std::size_t i = 0; i < N; i++)
        ptr[i] = make_texcoords(pixel_size, tile_count, i);
    return ptr;
}

} // namespace floormat
