#include "ground-atlas.hpp"
#include "quads.hpp"
#include "compat/assert.hpp"
#include "tile-image.hpp"
#include "compat/exception.hpp"
#include <limits>
#include <Magnum/Math/Color.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

using namespace floormat::Quads;

ground_atlas::ground_atlas(ground_def info, String path, const ImageView2D& image) :
    texcoords_{make_texcoords_array(Vector2ui(image.size()), info.size)},
    path_{std::move(path)}, name_{std::move(info.name)}, size_{image.size()}, dims_{info.size}, passability{info.pass}
{
    constexpr auto variant_max = std::numeric_limits<variant_t>::max();
    fm_soft_assert(num_tiles() <= variant_max);
    fm_soft_assert(dims_[0] > 0 && dims_[1] > 0);
    fm_soft_assert(size_ % Vector2ui{info.size} == Vector2ui());
    tex_.setLabel(path_)
        .setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Nearest)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(1)
        .setBorderColor(Color4{1, 0, 0, 1})
        .setStorage(1, GL::textureFormat(image.format()), image.size())
        .setSubImage(0, {}, image);
}

std::array<Vector2, 4> ground_atlas::texcoords_for_id(size_t i) const
{
    fm_assert(i < num_tiles());
    return texcoords_[i];
}

auto ground_atlas::make_texcoords(Vector2ui pixel_size, Vector2ub tile_count, size_t i) -> texcoords
{
    const auto sz = pixel_size/Vector2ui{tile_count};
    const auto id = Vector2ui{ uint32_t(i % tile_count[0]), uint32_t(i / tile_count[0]) };
    const auto p0 = id * sz;
    return texcoords_at(p0, sz, pixel_size);
}

auto ground_atlas::make_texcoords_array(Vector2ui pixel_size, Vector2ub tile_count) -> std::unique_ptr<const texcoords[]>
{
    const size_t N = Vector2ui{tile_count}.product();
    auto ptr = std::make_unique<std::array<Vector2, 4>[]>(N);
    for (auto i = 0uz; i < N; i++)
        ptr[i] = make_texcoords(pixel_size, tile_count, i);
    return ptr;
}

size_t ground_atlas::num_tiles() const { return Vector2ui{dims_}.product(); }
enum pass_mode ground_atlas::pass_mode() const { return passability; }

} // namespace floormat
