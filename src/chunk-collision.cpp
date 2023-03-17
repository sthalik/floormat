#include "chunk.hpp"
#include "RTree.hpp"
#include "tile-atlas.hpp"
#include "entity.hpp"
#include <bit>
#include <Corrade/Containers/PairStl.h>

namespace floormat {

chunk::RTree* chunk::rtree() noexcept { ensure_passability(); return &_rtree; }

namespace {

constexpr Vector2 tile_start(std::size_t k)
{
    constexpr auto half_tile = Vector2(TILE_SIZE2)/2;
    const local_coords coord{k};
    return TILE_SIZE2 * Vector2(coord) - half_tile;
}

Pair<Vector2i, Vector2i> scenery_tile(local_coords local, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size)
{
    auto center = iTILE_SIZE2 * Vector2i(local) + Vector2i(offset) + Vector2i(bbox_offset);
    auto min = center - Vector2i(bbox_size/2);
    auto size = Vector2i(bbox_size);
    return { min, min + size, };
}

constexpr Pair<Vector2, Vector2> whole_tile(std::size_t k)
{
    auto min = tile_start(k);
    return { min, min + TILE_SIZE2, };
}

constexpr Pair<Vector2, Vector2> wall_north(std::size_t k)
{
    auto min = tile_start(k) - Vector2(0, 1);
    return { min, min + Vector2(TILE_SIZE2[0], 2), };
}

constexpr Pair<Vector2, Vector2> wall_west(std::size_t k)
{
    auto min = tile_start(k) - Vector2(1, 0);
    return { min, min + Vector2(2, TILE_SIZE2[1]), };
}

constexpr std::uint64_t make_id(collision_type type, pass_mode p, std::uint64_t id)
{
    return std::bit_cast<std::uint64_t>(collision_data { (std::uint64_t)type, (std::uint64_t)p, id });
}

} // namespace

void chunk::ensure_passability() noexcept
{
    if (!_pass_modified)
        return;
    _pass_modified = false;

    _rtree.RemoveAll();

    for (const std::shared_ptr<entity>& s : entities())
    {
        bbox box;
        if (_bbox_for_scenery(*s, box))
            _add_bbox(box);
    }

    for (auto i = 0_uz; i < TILE_COUNT; i++)
    {
        if (const auto* atlas = ground_atlas_at(i))
            if (auto p = atlas->pass_mode(pass_mode::pass); p != pass_mode::pass)
            {
                auto [min, max] = whole_tile(i);
                auto id = make_id(collision_type::geometry, p, i);
                _rtree.Insert(min.data(), max.data(), id);
            }
    }
    for (auto i = 0_uz; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (const auto* atlas = tile.wall_north_atlas().get())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
            {
                auto [min, max] = wall_north(i);
                auto id = make_id(collision_type::geometry, p, i);
                _rtree.Insert(min.data(), max.data(), id);
            }
        if (const auto* atlas = tile.wall_west_atlas().get())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
            {
                auto [min, max] = wall_west(i);
                auto id = make_id(collision_type::geometry, p, i);
                _rtree.Insert(min.data(), max.data(), id);
            }
    }
}

bool chunk::_bbox_for_scenery(const entity& s, local_coords local, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept
{
    auto [start, end] = scenery_tile(local, offset, bbox_offset, bbox_size);
    auto id = make_id(collision_type::scenery, s.pass, s.id);
    value = { .id = id, .start = start, .end = end };
    return s.atlas && s.pass != pass_mode::pass;
}

bool chunk::_bbox_for_scenery(const entity& s, bbox& value) noexcept
{
    return _bbox_for_scenery(s, s.coord.local(), s.offset, s.bbox_offset, s.bbox_size, value);
}

void chunk::_remove_bbox(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree.Remove(start.data(), end.data(), x.id);
}

void chunk::_add_bbox(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree.Insert(start.data(), end.data(), x.id);
}

void chunk::_replace_bbox(const bbox& x0, const bbox& x1, bool b0, bool b1)
{
    if (_pass_modified)
        return;

    unsigned i = (unsigned)b1 << 1 | (unsigned)(b0 ? 1 : 0) << 0;
    CORRADE_ASSUME(i < 4u);

    switch (i) // NOLINT(hicpp-multiway-paths-covered)
    {
    case 1 << 1 | 1 << 0:
        if (x1 == x0)
            return;
        _remove_bbox(x0);
        [[fallthrough]];
    case 1 << 1 | 0 << 0:
        _add_bbox(x1);
        return;
    case 0 << 1 | 1 << 0:
        _remove_bbox(x0);
        return;
    case 0 << 1 | 0 << 0:
        return;
    }
    CORRADE_ASSUME(false);
}

bool chunk::can_place_entity(const entity_proto& proto, local_coords pos)
{
    (void)ensure_scenery_mesh();

    const auto center = Vector2(pos)*TILE_SIZE2 + Vector2(proto.offset) + Vector2(proto.bbox_offset),
               min = center - Vector2(proto.bbox_size/2), max = min + Vector2(proto.bbox_size);
    bool ret = true;
    _rtree.Search(min.data(), max.data(), [&](auto, const auto&) { return ret = false; });
    return ret;
}

} // namespace floormat
