#include "anim-atlas.hpp"
#include "compat/assert.hpp"
#include "shaders/tile.hpp"
#include "tile-defs.hpp"
#include <Corrade/Containers/BitArrayView.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/Math/Color.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

static constexpr const char name_array[][3] = { "n", "ne", "e", "se", "s", "sw", "w", "nw", };
static constexpr inline auto rot_count = std::size_t(rotation_COUNT);

static_assert(std::size(name_array) == rot_count);
static_assert(rot_count == 8);

std::uint8_t anim_atlas::rotation_to_index(StringView name) noexcept
{
    for (std::uint8_t i = 0; i < rot_count; i++)
        if (name == StringView{name_array[i]})
            return i;
    fm_abort("can't parse rotation name '%s'", name.data());
}

decltype(anim_atlas::_group_indices) anim_atlas::make_group_indices(const anim_def& a) noexcept
{
    std::array<std::uint8_t, rot_count> array = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    };
    const auto ngroups = a.groups.size();
    for (std::size_t i = 0; i < ngroups; i++)
        array[rotation_to_index(a.groups[i].name)] = std::uint8_t(i);
    return array;
}

anim_atlas::anim_atlas() noexcept = default;
anim_atlas::anim_atlas(StringView name, const ImageView2D& image, anim_def info) noexcept :
    _name{name}, _bitmask{make_bitmask(image)},
    _info{std::move(info)}, _group_indices{make_group_indices(_info)}
{
    fm_assert(!_info.groups.empty());

    const Size<3> size = image.pixels().size();
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
    const auto width = size[1], height = size[0], dest_len = width*height,
               stride = (std::size_t)pixels.stride()[0], width0 = width & ~7u;
    const auto* const data = (const unsigned char*)pixels.data();
    auto* const dest = new unsigned char[(dest_len+7)>>3];
    auto array = BitArray{dest, 0, dest_len, {}};

    fm_assert(tex.pixelSize() == 4);
    fm_assert(pixels.stride()[1] == 4);

    for (std::size_t j = 0; j < height; j++)
    {
        constexpr unsigned char amin = 32;
        std::size_t i = 0;
        for (; i < width0; i += 8)
        {
            const auto src_idx = (j*stride + i*4)+3, dst_idx = (height-j-1)*width + i>>3;
            const unsigned char* buf = data + src_idx;
            auto value = (unsigned char)(
                (unsigned char)(buf[0*4] >= amin) << 0 |
                (unsigned char)(buf[1*4] >= amin) << 1 |
                (unsigned char)(buf[2*4] >= amin) << 2 |
                (unsigned char)(buf[3*4] >= amin) << 3 |
                (unsigned char)(buf[4*4] >= amin) << 4 |
                (unsigned char)(buf[5*4] >= amin) << 5 |
                (unsigned char)(buf[6*4] >= amin) << 6 |
                (unsigned char)(buf[7*4] >= amin) << 7);
            dest[dst_idx] = value;
        }
        for (; i < width; i++)
        {
            unsigned char alpha = data[(j*stride + i*4)+3];
            array.set((height-j-1)*width + i, alpha >= amin);
        }
    }
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
    if (r < rotation_COUNT)
        for (auto i = ssize(r)-1; i >= 0; i--)
            if (_group_indices[std::size_t(i)] != 0xff)
                return rotation(i);
    for (auto i = count-1; i >= 0; i--)
        if (_group_indices[std::size_t(i)] != 0xff)
            return rotation(i);
    fm_abort("where did the rotations go?!");
}

bool anim_atlas::check_rotation(rotation r) const noexcept
{
    return r < rotation_COUNT && _group_indices[std::size_t(r)] < 0xff;
}

rotation anim_atlas::first_rotation() const noexcept
{
    for (std::size_t i = 0; i < rot_count; i++)
        if (_group_indices[i] == 0)
            return rotation(i);
    fm_abort("unreachable! can't find first rotation");
}

} // namespace floormat
