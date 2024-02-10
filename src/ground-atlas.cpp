#include "ground-atlas.hpp"
#include "quads.hpp"
#include "compat/assert.hpp"
#include "tile-image.hpp"
#include "compat/exception.hpp"
#include "loader/loader.hpp"
#include <limits>
#include <Magnum/Math/Color.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

using namespace floormat::Quads;

ground_atlas::ground_atlas(ground_def info, const ImageView2D& image) :
    _def{std::move(info)}, _path{make_path(_def.name)},
    _texcoords{make_texcoords_array(Vector2ui(image.size()), _def.size)},
    _pixel_size{image.size()}
{
    //Debug{} << "make ground_atlas" << _def.name;
    constexpr auto variant_max = std::numeric_limits<variant_t>::max();
    fm_soft_assert(num_tiles() <= variant_max);
    fm_soft_assert(_def.size.x() > 0 && _def.size.y() > 0);
    fm_soft_assert(_pixel_size % Vector2ui{_def.size} == Vector2ui());
    _tex.setLabel(_path)
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
    return _texcoords[i];
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

size_t ground_atlas::num_tiles() const { return Vector2ui{_def.size}.product(); }
enum pass_mode ground_atlas::pass_mode() const { return _def.pass; }

String ground_atlas::make_path(StringView name)
{
    char buf[fm_FILENAME_MAX];
    auto sv = loader.make_atlas_path(buf, loader.GROUND_TILESET_PATH, name);
    return String{sv};
}

} // namespace floormat
