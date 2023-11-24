#include "wall-atlas.hpp"
#include "compat/exception.hpp"
#include "src/tile-defs.hpp"
#include <utility>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

wall_atlas::wall_atlas() noexcept = default;
wall_atlas::~wall_atlas() noexcept = default;

#if 0
void wall_atlas::validate(const wall_atlas& a, const ImageView2D& img) noexcept(false)
{
    // todo

    const auto pixels = img.pixels();
    const auto size   = pixels.size();
    const auto width = size[1], height = size[0];

    fm_soft_assert(width * height > 0);

    for (const auto& frame : a.raw_frame_array())
    {
        fm_soft_assert(frame.offset < Vector2ui{(unsigned)width, (unsigned)height});
        fm_soft_assert((int)frame.offset.y() + iTILE_SIZE.z() <= (int)height);
        fm_soft_assert((int)frame.offset.x() < iTILE_SIZE2.x());
        // todo check frame offset + size based on wall_atlas::expected_size()
    }

    const auto frame_count = a.raw_frame_array().size();
    bool got_group = false;

    for (auto [_str, _group, tag] : Direction::groups)
    {
        const auto* dir = a.direction((size_t)tag);
        if (!dir)
            continue;
        const auto* g = a.group(dir, tag);
        if (!g)
            continue;
        got_group = true;
        fm_soft_assert(g->count > 0 == g->index < (uint32_t)-1);
        if (g->count > 0)
        {
            fm_soft_assert(g->index < frame_count);
            fm_soft_assert(g->index + g->count <= frame_count);
        }
    }
    fm_soft_assert(got_group);
}
#endif

Vector2i wall_atlas::expected_size(unsigned depth, Group_ group)
{
    static_assert(iTILE_SIZE2.x() == iTILE_SIZE2.y());
    constexpr int half_tile = iTILE_SIZE2.x()/2;

    fm_assert(depth > 0 && depth < 1<<16);
    CORRADE_ASSUME(group < Group_::COUNT);

    using enum Group_;
    switch (group)
    {
    case overlay:
    case wall:
        return { iTILE_SIZE.x(), iTILE_SIZE.z() };
    case top:
    case side:
        return { (int)depth, iTILE_SIZE.z() };
    case corner_L:
        return { half_tile, iTILE_SIZE.z() };
    case corner_R:
        return { iTILE_SIZE2.x() - half_tile, iTILE_SIZE.z() };
    default:
        fm_assert(false);
    }
}

wall_atlas::wall_atlas(wall_atlas_def def, String path, const ImageView2D& img)
    : _dir_array{std::move(def.direction_array)},
      _frame_array{std::move(def.frames)},
      _info{std::move(def.header)}, _path{std::move(path)},
      _direction_map{def.direction_map }
{
    _texture.setLabel(_path)
            .setWrapping(GL::SamplerWrapping::ClampToEdge)
            .setMagnificationFilter(GL::SamplerFilter::Nearest)
            .setMinificationFilter(GL::SamplerFilter::Linear)
            .setMaxAnisotropy(1) // todo?
            .setBorderColor(Color4{1, 0, 0, 1})
            .setStorage(1, GL::textureFormat(img.format()), img.size())
            .setSubImage(0, {}, img);
}

auto wall_atlas::get_Direction(Direction_ num) const -> Direction*
{
    fm_debug_assert(num < Direction_::COUNT);

    if (auto DAI = _direction_map[(uint8_t)num])
        return const_cast<Direction*>(&_dir_array[DAI.val]);
    else [[unlikely]]
        return {};
}

auto wall_atlas::frames(const Group& group) const -> ArrayView<const Frame>
{
    if (_frame_array.empty()) [[unlikely]]
        return {};
    const auto size = _frame_array.size(); (void)size;
    const auto index = group.index, count = group.count;
    fm_assert(index < size && index <= index + count && index + count <= size);
    return { &_frame_array[index], count };
}

auto wall_atlas::group(Direction_ d, Group_ tag) const -> const Group* { return group((size_t)d, tag); }
auto wall_atlas::group(size_t d, size_t tag) const -> const Group* { return group(d, (Group_)tag); }

auto wall_atlas::group(size_t dir, Group_ tag) const -> const Group*
{
    fm_assert(tag < Group_::COUNT);
    const auto* const set_ = direction(dir);
    if (!set_)
        return {};
    const auto& set = *set_;

    const auto memfn = set.groups[(size_t)tag].member;
    const Group& ret = set.*memfn;
    if (ret.is_empty())
        return {};
    return &ret;
}

auto wall_atlas::group(const Direction& dir, Group_ tag) const -> const Group*
{
    fm_assert(tag < Group_::COUNT);
    const auto memfn = dir.groups[(size_t)tag].member;
    const Group& ret = dir.*memfn;
    if (ret.is_empty())
        return {};
    return &ret;
}

auto wall_atlas::group(const Direction* dir, Group_ tag) const -> const Group*
{
    fm_debug_assert(dir != nullptr);
    return group(*dir, tag);
}

auto wall_atlas::direction(size_t dir) const -> const Direction*
{
    return get_Direction(Direction_(dir));
}

uint8_t wall_atlas::direction_count() const { return (uint8_t)_dir_array.size(); }
auto wall_atlas::raw_frame_array() const -> ArrayView<const Frame> { return _frame_array; }
GL::Texture2D& wall_atlas::texture() { fm_debug_assert(_texture.id()); return _texture; }

size_t wall_atlas::enum_to_index(enum rotation r)
{
    static_assert(rotation_COUNT == rotation{8});
    fm_debug_assert(r < rotation_COUNT);

    auto x = uint8_t(r);
    x >>= 1;
    return x;
}

} // namespace floormat

namespace floormat::Wall {

bool Direction::is_empty() const noexcept
{
    for (auto [str, member, tag] : Direction::groups)
        if (const auto& val = this->*member; !val.is_empty())
            return false;
    return true;
}

const Group& Direction::group(Group_ i) const { return const_cast<Direction&>(*this).group((size_t)i); }
const Group& Direction::group(size_t i) const { return const_cast<Direction&>(*this).group(i); }
Group& Direction::group(Group_ i) { return group((size_t)i); }

Group& Direction::group(size_t i)
{
    fm_assert(i < (size_t)Group_::COUNT);
    auto ptr = groups[i].member;
    return this->*ptr;
}

bool Frame::operator==(const Frame&) const noexcept = default;
bool Direction::operator==(const Direction&) const noexcept = default;
bool Info::operator==(const floormat::Wall::Info&) const noexcept = default;
bool DirArrayIndex::operator==(const floormat::Wall::DirArrayIndex&) const noexcept = default;

#if 1
bool Group::operator==(const Group&) const noexcept = default;
#else
bool Group::operator==(const Group& other) const noexcept
{
    bool ret = index == other.index && count == other.count &&
               pixel_size == other.pixel_size &&
               from_rotation == other.from_rotation &&
               mirrored == other.mirrored && default_tint == other.default_tint;

    if (!ret)
        return ret;

    constexpr auto eps = 1e-5f;
    if (auto diff = Math::abs(Vector4(tint_mult) - Vector4(other.tint_mult)).sum(); diff > eps)
        return false;
    if (auto diff = Math::abs(Vector3(tint_add) - Vector3(other.tint_add)).sum(); diff > eps)
        return false;

    return true;
}
#endif

} // namespace floormat::Wall
