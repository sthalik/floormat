#include "anim-atlas.hpp"
#include <Corrade/Containers/StringStlView.h>

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
anim_atlas::anim_atlas(StringView name, GL::Texture2D&& tex, anim_info info) noexcept :
    _tex{std::move(tex)}, _name{name},
    _info{std::move(info)}, _group_indices{make_group_indices(_info)}
{
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

auto anim_atlas::frame_texcoords(rotation r, std::size_t idx) const noexcept -> texcoords
{
    return frame_texcoords(frame(r, idx));
}

auto anim_atlas::frame_texcoords(const anim_frame& frame) const noexcept -> texcoords
{
    const Vector2 p0(frame.offset), p1(frame.offset + frame.size);
    const auto x0 = p0.x()+.5f, x1 = p1.x()-1, y0 = p0.y()+.5f, y1 = p1.y()-1;
    const auto size = _info.pixel_size;
    return {{
        { (x0+x1) / size[0], (y0+y1) / size[1]  }, // bottom right
        { (x0+x1) / size[0],      y0 / size[1]  }, // top right
        {      x0 / size[0], (y0+y1) / size[1]  }, // bottom left
        {      x0 / size[0],      y0 / size[1]  }, // top left
    }};
}

} // namespace floormat
