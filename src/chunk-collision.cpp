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

constexpr Pair<Vector2, Vector2> scenery_tile(std::size_t k, const scenery& sc)
{
    const local_coords coord{k};
    auto center = TILE_SIZE2 * Vector2(coord) + Vector2(sc.offset) + Vector2(sc.bbox_offset);
    auto start = center - Vector2(sc.bbox_size);
    auto size = Vector2(sc.bbox_size)*2;
    return { start, start + size, };
};

constexpr Pair<Vector2, Vector2> whole_tile(std::size_t k)
{
    auto start = tile_start(k);
    return { start, start + TILE_SIZE2, };
};

constexpr Pair<Vector2, Vector2> wall_north(std::size_t k)
{
    auto start = tile_start(k) - Vector2(0, 1);
    return { start, start + Vector2(TILE_SIZE2[0], 2), };
};

constexpr Pair<Vector2, Vector2> wall_west(std::size_t k)
{
    auto start = tile_start(k) - Vector2(1, 0);
    return { start, start + Vector2(2, TILE_SIZE2[1]), };
};

constexpr std::uint64_t make_id(collision_type type, std::uint64_t id)
{
    return std::bit_cast<std::uint64_t>(collision_data { (std::uint64_t)type, id });
}

} // namespace

void chunk::ensure_passability() noexcept
{
    if (!_pass_modified)
        return;
    _pass_modified = false;

    _rtree.RemoveAll();

    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (auto s = tile.scenery())
            if (s.frame.passability != pass_mode::pass && Vector2ui(s.frame.bbox_size).product() > 0)
            {
                auto [start, end] = scenery_tile(i, s.frame);
                auto id = make_id(collision_type::scenery, i);
                _rtree.Insert(start.data(), end.data(), id);
            }
    }
    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        if (const auto* atlas = ground_atlas_at(i))
            if (atlas->pass_mode(pass_mode::pass) != pass_mode::pass)
            {
                auto [start, end] = whole_tile(i);
                auto id = make_id(collision_type::geometry, i);
                _rtree.Insert(start.data(), end.data(), id);
            }
    }
    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (const auto* atlas = tile.wall_north_atlas().get())
            if (atlas->pass_mode(pass_mode::blocked) != pass_mode::pass)
            {
                auto [start, end] = wall_north(i);
                auto id = make_id(collision_type::geometry, i);
                _rtree.Insert(start.data(), end.data(), id);
            }
        if (const auto* atlas = tile.wall_west_atlas().get())
            if (atlas->pass_mode(pass_mode::blocked) != pass_mode::pass)
            {
                auto [start, end] = wall_west(i);
                auto id = make_id(collision_type::geometry, i);
                _rtree.Insert(start.data(), end.data(), id);
            }
    }
}

} // namespace floormat
