#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include "shaders/tile.hpp"
#include "tile-defs.hpp"
#include <Corrade/Containers/BitArrayView.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

static constexpr const char* name_array[] = { "n", "ne", "e", "se", "s", "sw", "w", "nw", };

std::uint8_t anim_atlas::rotation_to_index(const anim_def& info, rotation r) noexcept
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

decltype(anim_atlas::_group_indices) anim_atlas::make_group_indices(const anim_def& a) noexcept
{
    std::array<std::uint8_t, (std::size_t)rotation_COUNT> array;
    for (std::size_t i = 0; i < array.size(); i++)
        array[i] = rotation_to_index(a, rotation(i));
    return array;
}

anim_atlas::anim_atlas() noexcept = default;
anim_atlas::anim_atlas(StringView name, const ImageView2D& image, anim_def info) noexcept :
    _name{name}, _bitmask{make_bitmask(image)},
    _info{std::move(info)}, _group_indices{make_group_indices(_info)}
{
    const Size<3>& size = image.pixels().size();
    fm_assert(size[0]*size[1] == _info.pixel_size.product());
    fm_assert(size[2] >= 3 && size[2] <= 4);

    for (const auto pixel_size = _info.pixel_size;
         const auto& group : _info.groups)
        for (const auto& fr : group.frames)
        {
            fm_assert(fr.size.product() != 0);
            fm_assert(fr.offset < pixel_size);
            fm_assert(fr.offset + fr.size <= pixel_size);
        }

    _tex.setWrapping(GL::SamplerWrapping::ClampToEdge)
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

auto anim_atlas::texcoords_for_frame(rotation r, std::size_t i, bool mirror) const noexcept -> texcoords
{
    const auto f = frame(r, i);
    const Vector2 p0(f.offset), p1(f.size);
    const auto x0 = p0.x()+.5f, x1 = p1.x()-1, y0 = p0.y()+.5f, y1 = p1.y()-1;
    const auto size = _info.pixel_size;
    if (!mirror)
        return {{
            { (x0+x1) / size[0], 1 - (y0+y1) / size[1]  }, // bottom right
            { (x0+x1) / size[0], 1 -      y0 / size[1]  }, // top right
            {      x0 / size[0], 1 - (y0+y1) / size[1]  }, // bottom left
            {      x0 / size[0], 1 -      y0 / size[1]  }, // top left
        }};
    else
        return {{
            {      x0 / size[0], 1 - (y0+y1) / size[1]  }, // bottom right
            {      x0 / size[0], 1 -      y0 / size[1]  }, // top right
            { (x0+x1) / size[0], 1 - (y0+y1) / size[1]  }, // bottom left
            { (x0+x1) / size[0], 1 -      y0 / size[1]  }, // top left
        }};
}

auto anim_atlas::frame_quad(const Vector3& center, rotation r, std::size_t i) const noexcept -> quad
{
    enum : std::size_t { x, y, z };
    const auto f = frame(r, i);
    const auto gx = (float)f.ground[x]*.5f, gy = (float)f.ground[y]*.5f;
    const auto size = Vector2d(f.size);
    const auto sx = (float)size[x]*.5f, sy = (float)size[y]*.5f;
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
    const auto pixels = tex.pixels();
    const auto size   = pixels.size();
    const auto width  = size[1], height = size[0];

    if (tex.pixelSize() == 3)
        return BitArray{DirectInit, width*height, true};

    fm_assert(tex.pixelSize() == 4);
    fm_assert(pixels.stride()[1] == 4);
    const auto stride = (std::size_t)pixels.stride()[0];
    BitArray array{NoInit, width*height};
    const char* __restrict const data = (const char*)pixels.data();
    for (std::size_t y = 0; y < height; y++)
        for (std::size_t x = 0; x < width; x++)
            array.set(y*width + x, data[(height-y-1)*stride + x*4 + 3] != 0);
    return array;
}

BitArrayView anim_atlas::bitmask() const
{
    return _bitmask;
}

rotation anim_atlas::next_rotation_from(rotation r) const noexcept
{
    constexpr auto count = std::size_t(rotation_COUNT);
    for (auto i = std::size_t(r)+1; i < count; i++)
        if (_group_indices[i] != 0xff)
            return rotation(i);
    for (std::size_t i = 0; i < count; i++)
        if (_group_indices[i] != 0xff)
            return rotation(i);
    fm_abort("where did the rotations go?!");
}

rotation anim_atlas::prev_rotation_from(rotation r) const noexcept
{
    using ssize = std::make_signed_t<std::size_t>;
    constexpr auto count = ssize(rotation_COUNT);
    for (auto i = ssize(r)-1; i >= 0; i--)
        if (_group_indices[std::size_t(i)] != 0xff)
            return rotation(i);
    for (auto i = count-1; i >= 0; i--)
        if (_group_indices[std::size_t(i)] != 0xff)
            return rotation(i);
    fm_abort("where did the rotations go?!");
}

} // namespace floormat
