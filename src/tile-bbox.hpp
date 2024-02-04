#pragma once
#include "src/tile-defs.hpp"
#include "src/local-coords.hpp"
#include <Corrade/Containers/Pair.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

constexpr Vector2 tile_start(size_t k)
{
    constexpr auto half_tile = Vector2(TILE_SIZE2)/2;
    const local_coords coord{k};
    return TILE_SIZE2 * Vector2(coord) - half_tile;
}

constexpr Pair<Vector2i, Vector2i> scenery_tile(local_coords local, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size)
{
    auto center = iTILE_SIZE2 * Vector2i(local) + Vector2i(offset) + Vector2i(bbox_offset);
    auto min = center - Vector2i(bbox_size/2);
    auto size = Vector2i(bbox_size);
    return { min, min + size, };
}

constexpr Pair<Vector2, Vector2> whole_tile(size_t k)
{
    auto min = tile_start(k);
    return { min, min + TILE_SIZE2, };
}

constexpr Pair<Vector2, Vector2> wall_north(size_t k, float wall_depth)
{
    constexpr auto eps = Vector2(0.25f);
    auto min = tile_start(k) - Vector2{0, wall_depth};
    return { min - eps, min + Vector2{TILE_SIZE2.x(), wall_depth} + eps*2, };
}

constexpr Pair<Vector2, Vector2> wall_west(size_t k, float wall_depth)
{
    constexpr auto eps = Vector2(0.25f);
    auto min = tile_start(k) - Vector2{wall_depth, 0};
    return { min - eps, min + Vector2{wall_depth, TILE_SIZE2.y()} + eps*2, };
}

} // namespace floormat
