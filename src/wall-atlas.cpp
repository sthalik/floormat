#include "wall-atlas.hpp"
#include "compat/exception.hpp"
#include "src/tile-defs.hpp"
#include <utility>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

wall_atlas::wall_atlas() noexcept = default;
wall_atlas::~wall_atlas() noexcept = default;

void wall_atlas::validate(const wall_atlas& a, const ImageView2D& img) noexcept(false)
{
    const auto pixels = img.pixels();
    const auto size   = pixels.size();
    const auto width = size[1], height = size[0];

    fm_soft_assert(width * height > 0);

    for (const auto& frame : a.raw_frame_array())
    {
        fm_soft_assert(frame.offset < Vector2ui{(unsigned)width, (unsigned)height});
        // todo check frame offset + size
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

Vector2i wall_atlas::expected_size(int depth, Tag group)
{
    static_assert(iTILE_SIZE2.x() == iTILE_SIZE2.y());
    constexpr int half_tile = iTILE_SIZE2.x()/2;
    CORRADE_ASSUME(group < Tag::COUNT);
    using enum Tag;
    switch (group)
    {
    case overlay:
    case wall:
        return { iTILE_SIZE.x(), iTILE_SIZE.z() };
    case top:
    case side:
        return { depth, iTILE_SIZE.z() };
    case corner_L:
        return { half_tile, iTILE_SIZE.z() };
    case corner_R:
        return { iTILE_SIZE2.x() - half_tile, iTILE_SIZE.z() };
    default:
        fm_assert(false);
    }
}

wall_atlas::wall_atlas(Info info, const ImageView2D& img,
                       Array<Frame> frames,
                       Array<Direction> directions,
                       std::array<DirArrayIndex, 4> direction_to_DirArrayIndex)
    : _dir_array{ std::move(directions) }, _frame_array{ std::move(frames) },
      _info{ std::move(info) },
      _direction_to_Direction_array_index{ direction_to_DirArrayIndex }
{
    _texture.setLabel(_info.name)
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
    constexpr DirArrayIndex default_DAI;
    fm_debug_assert(num < Direction_::COUNT);

    if (_dir_array.isEmpty()) [[unlikely]]
        return {};
    else if (auto DAI = _direction_to_Direction_array_index[(uint8_t)num]; DAI != default_DAI) [[likely]]
        return const_cast<Direction*>(&_dir_array[DAI.val]);
    else
        return {};
}

auto wall_atlas::frames(const Group& group) const -> ArrayView<const Frame>
{
    if (_frame_array.isEmpty()) [[unlikely]]
        return {};
    const auto size = _frame_array.size(); (void)size;
    const auto index = group.index, count = group.count;
    fm_assert(index < size && index <= index + count && index + count <= size);
    return { &_frame_array[index], count };
}

auto wall_atlas::group(size_t dir, Tag tag) const -> const Group*
{
    fm_assert(tag < Tag::COUNT);
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

auto wall_atlas::group(const Direction& dir, Tag tag) const -> const Group*
{
    fm_assert(tag < Tag::COUNT);
    const auto memfn = dir.groups[(size_t)tag].member;
    const Group& ret = dir.*memfn;
    if (ret.is_empty())
        return {};
    return &ret;
}

auto wall_atlas::group(const Direction* dir, Tag tag) const -> const Group*
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
auto wall_atlas::info() const -> const Info& { return _info; }
GL::Texture2D& wall_atlas::texture() { fm_debug_assert(_texture.id()); return _texture; }
StringView wall_atlas::name() const { return _info.name; }

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
