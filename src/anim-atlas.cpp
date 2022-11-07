#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include "shaders/tile.hpp"
#include <Magnum/Math/Color.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

static constexpr std::array<char[3], (std::size_t)rotation::COUNT> name_array = {
    "n", "ne", "e", "se", "s", "sw", "w", "nw",
};

std::uint8_t anim_atlas::rotation_to_index(const anim_info& info, rotation r) noexcept
{
    StringView str = name_array[std::size_t(r)];
    for (std::size_t sz = info.groups.size(), i = 0; i < sz; i++)
    {
        const anim_group& g = info.groups[i];
        if (g.name == str)
            return std::uint8_t(i);
    }
    return 0xff;
}

decltype(anim_atlas::_group_indices) anim_atlas::make_group_indices(const anim_info& a) noexcept
{
    std::array<std::uint8_t, (std::size_t)rotation::COUNT> array;
    for (std::size_t i = 0; i < array.size(); i++)
        array[i] = rotation_to_index(a, rotation(i));
    return array;
}

anim_atlas::anim_atlas() noexcept = default;
anim_atlas::anim_atlas(StringView name, const ImageView2D& image, anim_info info) noexcept :
    _name{name},
    _info{std::move(info)}, _group_indices{make_group_indices(_info)}
{
    _tex.setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Nearest)
        .setMinificationFilter(GL::SamplerFilter::Linear)
        .setMaxAnisotropy(1)
        .setBorderColor(Color4{1, 0, 0, 1})
        .setStorage(1, GL::textureFormat(image.format()), image.size())
        .setSubImage(0, {}, image);
}

anim_atlas::~anim_atlas() noexcept = default;
anim_atlas::anim_atlas(anim_atlas&&) noexcept = default;
anim_atlas& anim_atlas::operator=(anim_atlas&&) noexcept = default;

StringView anim_atlas::name() const noexcept { return _name; }
GL::Texture2D& anim_atlas::texture() noexcept { return _tex; }
const Serialize::anim& anim_atlas::info() const noexcept { return _info; }

auto anim_atlas::group(rotation r) const noexcept -> const anim_group&
{
    const auto group_idx = _group_indices[std::size_t(r)];
    fm_assert(group_idx != 0xff);
    return _info.groups[group_idx];
}

auto anim_atlas::frame(rotation r, std::size_t frame) const noexcept -> const anim_frame&
{
    const anim_group& g = group(r);
    fm_assert(frame < g.frames.size());
    return g.frames[frame];
}

auto anim_atlas::texcoords_for_frame(rotation r, std::size_t i) const noexcept -> texcoords
{
    return texcoords_for_frame(frame(r, i));
}

auto anim_atlas::texcoords_for_frame(const anim_frame& frame) const noexcept -> texcoords
{
    const Vector2 p0(frame.offset), p1(frame.size);
    const auto x0 = p0.x()+.5f, x1 = p1.x()-1, y0 = p0.y()+.5f, y1 = p1.y()-1;
    const auto size = _info.pixel_size;
    return {{
        { (x0+x1) / size[0], 1 - (y0+y1) / size[1]  }, // bottom right
        { (x0+x1) / size[0], 1 -      y0 / size[1]  }, // top right
        {      x0 / size[0], 1 - (y0+y1) / size[1]  }, // bottom left
        {      x0 / size[0], 1 -      y0 / size[1]  }, // top left
    }};
}

auto anim_atlas::frame_quad(const Vector3& center, rotation r, std::size_t i) const noexcept -> quad
{
    return frame_quad(center, frame(r, i));
}

auto anim_atlas::frame_quad(const Vector3& center, const anim_frame& frame) noexcept -> quad
{
    const auto size = Vector2d(frame.size);
    const double gx = frame.ground[0]*.25, gy = frame.ground[1]*.25;
    const double sx = size[0]*.25, sy = size[1]*.25;
    const auto bottom_right = Vector2(tile_shader::unproject({  sx - gx,  sy - gy })),
               top_right    = Vector2(tile_shader::unproject({  sx - gx, -sy - gy })),
               bottom_left  = Vector2(tile_shader::unproject({ -sx - gx,  sy - gy })),
               top_left     = Vector2(tile_shader::unproject({ -sx - gx, -sy - gy }));
    const auto cx = center[0], cy = center[1], cz = center[2];
    return {{
        { cx + bottom_right[0], cy + bottom_right[1],   cz },
        { cx + top_right[0],    cy + top_right[1],      cz },
        { cx + bottom_left[0],  cy + bottom_left[1],    cz },
        { cx + top_left[0],     cy + top_left[1],       cz },
    }};
}

} // namespace floormat
