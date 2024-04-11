#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include "shaders/shader.hpp"
#include "compat/exception.hpp"
#include <Corrade/Containers/BitArrayView.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

static constexpr const char name_array[][3] = { "n", "ne", "e", "se", "s", "sw", "w", "nw", };
static constexpr inline auto rot_count = size_t(rotation_COUNT);

static_assert(std::size(name_array) == rot_count);
static_assert(rot_count == 8);

uint8_t anim_atlas::rotation_to_index(StringView name)
{
    for (uint8_t i = 0; i < rot_count; i++)
        if (name == StringView{name_array[i]})
            return i;
    fm_throw("can't parse rotation name '{}'"_cf, name);
}

decltype(anim_atlas::_group_indices) anim_atlas::make_group_indices(const anim_def& a) noexcept
{
    std::array<uint8_t, rot_count> array = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    const auto ngroups = a.groups.size();
    for (auto i = 0uz; i < ngroups; i++)
        array[rotation_to_index(a.groups[i].name)] = uint8_t(i);
    return array;
}

anim_atlas::anim_atlas() noexcept = default;
anim_atlas::anim_atlas(String name, const ImageView2D& image, anim_def info) :
    _name{move(name)}, _bitmask{make_bitmask(image)},
    _info{move(info)}, _group_indices{make_group_indices(_info)}
{
    fm_soft_assert(!_info.groups.isEmpty());

    const Size<3> size = image.pixels().size();
    fm_soft_assert(size[0]*size[1] == _info.pixel_size.product());
    fm_soft_assert(size[2] >= 3 && size[2] <= 4);

    for (const auto pixel_size = _info.pixel_size;
         const auto& group : _info.groups)
        for (const auto& fr : group.frames)
        {
            fm_soft_assert(fr.size.product() != 0);
            fm_soft_assert(fr.offset < pixel_size);
            fm_soft_assert(fr.offset + fr.size <= pixel_size);
        }

    _tex.setLabel(_name)
        .setWrapping(GL::SamplerWrapping::ClampToEdge)
        .setMagnificationFilter(GL::SamplerFilter::Nearest)
        .setMinificationFilter(GL::SamplerFilter::Nearest)
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
const anim_def& anim_atlas::info() const noexcept { return _info; }

auto anim_atlas::group(rotation r) const -> const anim_group&
{
    const auto group_idx = _group_indices[size_t(r)];
    fm_soft_assert(group_idx != 0xff);
    return _info.groups[group_idx];
}

auto anim_atlas::frame(rotation r, size_t frame) const -> const anim_frame&
{
    const anim_group& g = group(r);
    fm_soft_assert(frame < g.frames.size());
    return g.frames[frame];
}

// get uv's for atlas frame
// ORDER: bottom left, top right, bottom left, top left
auto anim_atlas::texcoords_for_frame(rotation r, size_t i, bool mirror) const noexcept -> texcoords
{
    const auto f = frame(r, i);
    const Vector2 p0(f.offset), p1(f.size);
    const auto x0 = p0.x()+.5f, x1 = p1.x()-1, y0 = p0.y()+.5f, y1 = p1.y()-1;
    const auto size = _info.pixel_size;
    if (!mirror)
        return {{
            { (x0+x1) / size.x(), 1 - (y0+y1) / size.y()  }, // bottom right
            { (x0+x1) / size.x(), 1 -      y0 / size.y()  }, // top right
            {      x0 / size.x(), 1 - (y0+y1) / size.y()  }, // bottom left
            {      x0 / size.x(), 1 -      y0 / size.y()  }, // top left
        }};
    else
        return {{
            {      x0 / size.x(), 1 - (y0+y1) / size.y() }, // bottom right
            {      x0 / size.x(), 1 -      y0 / size.y() }, // top right
            { (x0+x1) / size.x(), 1 - (y0+y1) / size.y() }, // bottom left
            { (x0+x1) / size.x(), 1 -      y0 / size.y() }, // top left
        }};
}

// get vertexes for atlas frame
// ORDER: bottom left, top right, bottom left, top left
auto anim_atlas::frame_quad(const Vector3& center, rotation r, size_t i) const noexcept -> quad
{
    enum : size_t { x, y, z };
    const auto f = frame(r, i);
    const auto gx = f.ground[x]*.5f, gy = f.ground[y]*.5f;
    const auto size = Vector2(f.size);
    const auto sx = size[x]*.5f, sy = size[y]*.5f;
    const auto bottom_right = tile_shader::unproject({  sx - gx,  sy - gy }),
               top_right    = tile_shader::unproject({  sx - gx,     - gy }),
               bottom_left  = tile_shader::unproject({     - gx,  sy - gy }),
               top_left     = tile_shader::unproject({     - gx,     - gy });
    const auto c = center + Vector3(group(r).offset);
    return {{
        { c[x] + bottom_right[x], c[y] + bottom_right[y],  c[z] },
        { c[x] + top_right[x],    c[y] + top_right[y],     c[z] },
        { c[x] + bottom_left[x],  c[y] + bottom_left[y],   c[z] },
        { c[x] + top_left[x],     c[y] + top_left[y],      c[z] },
    }};
}

BitArray anim_atlas::make_bitmask(const ImageView2D& tex)
{
    if (tex.pixelSize() == 3)
        return {};

    const auto size = tex.pixels().size();
    auto width = (size[0]+7)&~7uz;
    auto array = BitArray{NoInit, width*size[1]};
    make_bitmask_(tex, array);
    return array;
}

BitArrayView anim_atlas::bitmask() const
{
    return _bitmask;
}

rotation anim_atlas::next_rotation_from(rotation r) const noexcept
{
    constexpr auto count = size_t(rotation_COUNT);
    for (auto i = size_t(r)+1; i < count; i++)
        if (_group_indices[i] != 0xff)
            return rotation(i);
    for (auto i = 0uz; i < count; i++)
        if (_group_indices[i] != 0xff)
            return rotation(i);
    fm_abort("where did the rotations go?!");
}

rotation anim_atlas::prev_rotation_from(rotation r) const noexcept
{
    using ssize = std::make_signed_t<size_t>;
    constexpr auto count = ssize(rotation_COUNT);
    if (r < rotation_COUNT)
        for (auto i = ssize(r)-1; i >= 0; i--)
            if (_group_indices[size_t(i)] != 0xff)
                return rotation(i);
    for (auto i = count-1; i >= 0; i--)
        if (_group_indices[size_t(i)] != 0xff)
            return rotation(i);
    fm_abort("where did the rotations go?!");
}

bool anim_atlas::check_rotation(rotation r) const noexcept
{
    return r < rotation_COUNT && _group_indices[size_t(r)] < 0xff;
}

rotation anim_atlas::first_rotation() const noexcept
{
    for (auto i = 0uz; i < rot_count; i++)
        if (_group_indices[i] == 0)
            return rotation(i);
    fm_abort("unreachable! can't find first rotation");
}

} // namespace floormat
