#include "chunk.hpp"
#include "RTree.hpp"
#include "tile-atlas.hpp"
#include <bit>
#include <Corrade/Containers/PairStl.h>

namespace floormat {

const chunk::RTree* chunk::rtree() const noexcept { return &_rtree; }
chunk::RTree* chunk::rtree() noexcept { return &_rtree; }

namespace {

constexpr Vector2 tile_start(std::size_t k)
{
    constexpr auto half_tile = Vector2(TILE_SIZE2)/2;
    const local_coords coord{k};
    return TILE_SIZE2 * Vector2(coord) - half_tile;
}

constexpr Pair<Vector2i, Vector2i> scenery_tile(std::size_t k, const scenery& sc)
{
    const local_coords coord{k};
    auto center = iTILE_SIZE2 * Vector2i(coord) + Vector2i(sc.offset) + Vector2i(sc.bbox_offset);
    auto start = center - Vector2i(sc.bbox_size);
    auto size = Vector2i(sc.bbox_size)*2;
    return { start, start + size, };
}

constexpr Pair<Vector2, Vector2> whole_tile(std::size_t k)
{
    auto start = tile_start(k);
    return { start, start + TILE_SIZE2, };
}

constexpr Pair<Vector2, Vector2> wall_north(std::size_t k)
{
    auto start = tile_start(k) - Vector2(0, 1);
    return { start, start + Vector2(TILE_SIZE2[0], 2), };
}

constexpr Pair<Vector2, Vector2> wall_west(std::size_t k)
{
    auto start = tile_start(k) - Vector2(1, 0);
    return { start, start + Vector2(2, TILE_SIZE2[1]), };
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

    for (auto i = 0_uz; i < TILE_COUNT; i++)
    {
        if (auto s = operator[](i).scenery())
        {
            bbox box;
            if (_bbox_for_scenery(i, box))
                _add_bbox(box);
        }
    }
    for (auto i = 0_uz; i < TILE_COUNT; i++)
    {
        if (const auto* atlas = ground_atlas_at(i))
            if (auto p = atlas->pass_mode(pass_mode::pass); p != pass_mode::pass)
            {
                auto [start, end] = whole_tile(i);
                auto id = make_id(collision_type::geometry, p, i);
                _rtree.Insert(start.data(), end.data(), id);
            }
    }
    for (auto i = 0_uz; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (const auto* atlas = tile.wall_north_atlas().get())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
            {
                auto [start, end] = wall_north(i);
                auto id = make_id(collision_type::geometry, p, i);
                _rtree.Insert(start.data(), end.data(), id);
            }
        if (const auto* atlas = tile.wall_west_atlas().get())
            if (auto p = atlas->pass_mode(pass_mode::blocked); p != pass_mode::pass)
            {
                auto [start, end] = wall_west(i);
                auto id = make_id(collision_type::geometry, p, i);
                _rtree.Insert(start.data(), end.data(), id);
            }
    }
}

bool chunk::_bbox_for_scenery(std::size_t i, bbox& value) noexcept
{
    fm_debug_assert(i < TILE_COUNT);
    auto [atlas, s] = operator[](i).scenery();
    auto [start, end] = scenery_tile(i, s);
    auto id = make_id(collision_type::scenery, s.passability, i);
    value = { .id = id, .start = start, .end = end };
    return atlas && s.passability != pass_mode::pass && Vector2i(s.bbox_size).product() > 0;
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

} // namespace floormat
