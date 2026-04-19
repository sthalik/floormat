#include "wall-atlas.hpp"
#include "tile-constants.hpp"
#include "compat/array-size.hpp"
#include "compat/exception.hpp"
#include "compat/borrowed-ptr.inl"
#include "loader/loader.hpp"
#include <cr/GrowableArray.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Magnum/ImageView.h>
#include <Magnum/PixelStorage.h>

namespace floormat {

template class bptr<wall_atlas>;
template class bptr<const wall_atlas>;

} // namespace floormat

namespace floormat::Wall {

uint8_t direction_index_from_name(StringView s) noexcept(false)
{
    for (uint8_t i = 0; auto [n, _] : wall_atlas::directions)
        if (n == s)
            return i;
        else
            i++;

    fm_throw("bad rotation name '{}'"_cf, s);
}

StringView direction_index_to_name(size_t i) noexcept(false)
{
    fm_soft_assert(i < array_size(wall_atlas::directions));
    return wall_atlas::directions[i].name;
}

void resolve_wall_rotations(Array<Wall::Direction>& array, const std::array<DirArrayIndex, Direction_COUNT>& map) noexcept(false)
{
    for (auto [dir_name, dir] : wall_atlas::directions)
    {
        auto DAI = map[(size_t)dir];
        if (!DAI)
            continue;
        auto& D = array[DAI.val];
        for (auto [group_name, ptr, gr] : Direction::groups)
        {
            auto& G = D.*ptr;
            if (!G.is_defined)
                continue;
            if (G.from_rotation != (uint8_t)-1)
            {
                const auto& DAI2 = map[G.from_rotation];
                if (!DAI2)
                    fm_throw("from_rotation for '{}/{}' points to nonexistent rotation {}"_cf,
                             dir_name, group_name, direction_index_to_name(G.from_rotation));
                const auto& D2 = array[DAI2.val];
                const auto& G2 = D2.*ptr;
                if (!G2.is_defined || G2.from_rotation != (uint8_t)-1)
                    fm_throw("from_rotation for '{}/{}' points to empty group '{}/{}'"_cf,
                             dir_name, group_name, direction_index_to_name(G.from_rotation), group_name);
                G.from_rotation = DAI2.val;
            }
        }
    }
}

} // namespace floormat::Wall

namespace floormat {

using namespace floormat::Wall;

wall_atlas::wall_atlas() noexcept = default;
wall_atlas::~wall_atlas() noexcept = default;

Vector2ui wall_atlas::expected_size(unsigned depth, Group_ group)
{
    constexpr auto size = Vector3ui{iTILE_SIZE};
    static_assert(size.x() == size.y());

    fm_assert(depth > 0 && depth < 1<<15);
    CORRADE_ASSUME(group < Group_::COUNT);

    switch (group)
    {
    using enum Group_;
    case wall:
        return { size.x(), size.z() };
    case top:
        return { depth, size.x() };
    case side:
    case corner:
        return { depth, size.z() };
    case COUNT:
        break;
    }
    fm_assert(false);
}

wall_atlas::wall_atlas(wall_atlas_def def, String path, const ImageView2D& img)
    : _dir_array{move(def.direction_array)},
      _frame_array{move(def.frames)},
      _info{move(def.header)}, _path{move(path)},
      _direction_map{def.direction_map}
{
    //Debug{} << "make wall_atlas" << _info.name;
    const Vector2ui img_size{(uint32_t)img.size().x(), (uint32_t)img.size().y()};
    fm_soft_assert(img.pixelSize() >= 3 && img.pixelSize() <= 4);
    fm_soft_assert(img_size.product() > 0);
    {
        const auto frame_count = _frame_array.size();
        fm_soft_assert(frame_count > 0);
        bool found = false;
        for (auto [dir_name, dir] : wall_atlas::directions)
        {
            const auto* D = direction((size_t)dir);
            fm_soft_assert(!!D == def.direction_mask[(size_t)dir]);
            if (!D)
                continue;
            for (auto [group_name, gmemb, gr] : Direction::groups)
            {
                const auto& G = D->*gmemb;
                fm_soft_assert(G.is_defined == !!G.count);
                fm_soft_assert(G.is_defined == (G.index != (uint32_t)-1));
                fm_soft_assert(G.from_rotation == (uint8_t)-1 || G.is_defined);
                if (!G.is_defined)
                    continue;
                found = true;
                fm_soft_assert(G.index < frame_count && G.index + G.count <= frame_count);
                const auto size = expected_size(_info.depth, gr);
                for (const auto& frame : ArrayView { &_frame_array[G.index], G.count })
                {
                    fm_soft_assert(frame.size == size);
                    fm_soft_assert(frame.offset + frame.size <= img_size);
                }
            }
        }
        if (!found) [[unlikely]]
            fm_throw("wall_atlas '{}' is empty!"_cf, _path);
    }

    {
        Array<bool> top_frame_mask{ValueInit, _frame_array.size()};
        for (const Direction& D : _dir_array)
        {
            // Only a group without from_rotation owns its frame range.
            // Mirrored directions point at the source; iterating the source
            // direction alone covers their frames.
            if (!D.top.is_defined || D.top.from_rotation != (uint8_t)-1)
                continue;
            const auto lo = D.top.index;
            const auto hi = lo + D.top.count;
            fm_assert(hi <= _frame_array.size());
            for (auto i = lo; i < hi; i++)
                top_frame_mask[i] = true;
        }

        const uint32_t full_height = img_size.y();
        arrayReserve(_frame_sprites, _frame_array.size());
        for (size_t fi = 0; fi < _frame_array.size(); fi++)
        {
            const Frame& f = _frame_array[fi];
            const uint32_t fw = f.size.x();
            const uint32_t fh = f.size.y();
            // Y-flip: Magnum bottom-up, JSON top-down. See loader/anim-traits.cpp.
            const uint32_t mem_y_start = full_height - f.offset.y() - fh;
            PixelStorage sub_storage = img.storage();
            sub_storage.setRowLength((Int)img_size.x())
                       .setSkip({(Int)f.offset.x(), (Int)mem_y_start, 0});
            const ImageView2D view{sub_storage, img.format(),
                                   {(Int)fw, (Int)fh}, img.data()};
            arrayAppend(_frame_sprites, loader.atlas().add(view, true));
        }
    }

    resolve_wall_rotations(_dir_array, _direction_map);
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
    if (_frame_array.isEmpty()) [[unlikely]]
        return {};
    const auto size = _frame_array.size(); (void)size;
    const auto index = group.index, count = group.count;
    fm_assert(index < size && index <= index + count && index + count <= size);
    return { &_frame_array[index], count };
}

auto wall_atlas::sprites(const Group& group) const -> ArrayView<const sprite>
{
    if (_frame_sprites.isEmpty()) [[unlikely]]
        return {};
    const auto size = _frame_sprites.size(); (void)size;
    const auto index = group.index, count = group.count;
    fm_assert(index < size && index <= index + count && index + count <= size);
    return { &_frame_sprites[index], count };
}

auto wall_atlas::frames(Direction_ dir, Group_ gr) const -> ArrayView<const Frame>
{
    const auto* D = get_Direction(dir);
    if (!D) [[unlikely]]
        fm_throw("no such direction: {}"_cf, (int)dir);
    const auto* G = group(*D, gr);
    if (!G) [[unlikely]]
        fm_throw("no such group {} for direction {}"_cf, (int)gr, (int)dir);
    return { _frame_array.data() + G->index, G->count };
}

auto wall_atlas::group(Direction_ dir, Group_ gr) const -> const Group* { return group((size_t)dir, (size_t)gr); }
auto wall_atlas::group(size_t dir, Group_ gr) const -> const Group* { return group(dir, (size_t)gr); }

auto wall_atlas::group(size_t dir, size_t tag) const -> const Group*
{
    fm_assert(tag < Group_COUNT);
    if (const auto* D = direction(dir))
        return group(*D, (Group_)tag);
    else
        return {};
}

auto wall_atlas::group(const Direction& dir, Group_ tag) -> const Group*
{
    fm_assert(tag < Group_::COUNT);
    const auto memfn = dir.groups[(size_t)tag].member;
    if (const Group& ret = dir.*memfn; ret.is_defined)
        return &ret;
    else
        return {};
}

auto wall_atlas::direction(size_t dir) const -> const Direction* { return get_Direction(Direction_(dir)); }
auto wall_atlas::direction(Direction_ dir) const -> const Direction* { return get_Direction(dir); }

auto wall_atlas::calc_direction(Direction_ dir) const -> const Direction&
{
    if (auto dai = _direction_map[(size_t)dir]) [[likely]]
        return _dir_array[dai.val];
    CORRADE_ASSUME(dir < Direction_::COUNT);
    Direction_ other;
    switch (dir)
    {
    case Direction_::N: other = Direction_::W; break;
    case Direction_::W: other = Direction_::N; break;
    default: other = Direction_::COUNT;
    }
    if (other != Direction_::COUNT)
        if (auto dai = _direction_map[(size_t)Direction_::N])
            return _dir_array[dai.val];
    fm_abort("wall_atlas: can't find direction '%d'", (int)dir);
}

uint8_t wall_atlas::direction_count() const { return (uint8_t)_dir_array.size(); }
auto wall_atlas::raw_frame_array() const -> ArrayView<const Frame> { return _frame_array; }
auto wall_atlas::raw_sprite_array() const -> ArrayView<const sprite> { return _frame_sprites; }

Vector2ui wall_atlas::image_size() const
{
    if (_frame_sprites.isEmpty()) [[unlikely]]
        return {};
    const auto& s = _frame_sprites[0];
    return { s.width(), s.height() };
}

} // namespace floormat

namespace floormat::Wall {

const Group& Direction::group(Group_ i) const { return const_cast<Direction&>(*this).group((size_t)i); }
const Group& Direction::group(size_t i) const { return const_cast<Direction&>(*this).group(i); }
Group& Direction::group(Group_ i) { return group((size_t)i); }

Group& Direction::group(size_t i)
{
    fm_assert(i < Group_COUNT);
    auto ptr = groups[i].member;
    return this->*ptr;
}

bool Frame::operator==(const Frame&) const noexcept = default;
bool Direction::operator==(const Direction&) const noexcept = default;
bool Info::operator==(const Info&) const noexcept = default;
bool DirArrayIndex::operator==(const DirArrayIndex&) const noexcept = default;
bool Group::operator==(const Group&) const noexcept = default;

} // namespace floormat::Wall
