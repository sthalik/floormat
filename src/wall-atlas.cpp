#include "wall-atlas.hpp"
#include "compat/assert.hpp"
#include <utility>
#include <Magnum/ImageView.h>
#include <Magnum/GL/TextureFormat.h>

namespace floormat {

namespace Wall {

} // namespace Wall

wall_atlas::wall_atlas() noexcept = default;
wall_atlas::~wall_atlas() noexcept = default;

wall_atlas::wall_atlas(Info info, const ImageView2D& image,
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
            .setStorage(1, GL::textureFormat(image.format()), image.size())
            .setSubImage(0, {}, image);
}

auto wall_atlas::get_Direction(Direction_ num) const -> Direction*
{
    fm_debug_assert(num < Direction_::COUNT);

    if (_dir_array.isEmpty()) [[unlikely]]
        return {};
    if (auto idx = _direction_to_Direction_array_index[(uint8_t)num])
        return const_cast<Direction*>(&_dir_array[idx.val]);
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

    const auto memfn = set.members[(size_t)tag].member;
    const Group& ret = set.*memfn;
    if (ret.is_empty())
        return {};
    return &ret;
}

auto wall_atlas::group(const Direction& dir, Tag tag) const -> const Group*
{
    fm_assert(tag < Tag::COUNT);
    const auto memfn = dir.members[(size_t)tag].member;
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
    for (auto [str, member, tag] : Direction::members)
        if (const auto& val = this->*member; !val.is_empty())
            return false;
    return true;
}

bool Frame::operator==(const Frame&) const noexcept = default;
bool Group::operator==(const Group&) const noexcept = default;
bool Direction::operator==(const Direction&) const noexcept = default;
bool Info::operator==(const floormat::Wall::Info&) const noexcept = default;
bool DirArrayIndex::operator==(const floormat::Wall::DirArrayIndex&) const noexcept = default;

} // namespace floormat::Wall
