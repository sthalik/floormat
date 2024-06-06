#pragma once
#include "tile-constants.hpp"
#include "src/local-coords.hpp"
#include <Corrade/Containers/Pair.h>
#include <Magnum/DimensionTraits.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

template<typename T = float>
constexpr Vector2 tile_start(size_t k)
{
    constexpr auto half_tile = VectorTypeFor<2,T>(tile_size_xy/2);
    const local_coords coord{k};
    return TILE_SIZE2 * VectorTypeFor<2,T>(coord) - half_tile;
}

constexpr Pair<Vector2i, Vector2i> scenery_tile(local_coords local, Vector2b offset, Vector2b bbox_offset, Vector2ub bbox_size)
{
    auto center = iTILE_SIZE2 * Vector2i(local) + Vector2i(offset) + Vector2i(bbox_offset);
    auto min = center - Vector2i(bbox_size/2);
    auto size = Vector2i(bbox_size);
    return { min, min + size, };
}

template<typename T = float>
constexpr Pair<VectorTypeFor<2,T>, VectorTypeFor<2,T>> whole_tile(size_t k)
{
    auto min = tile_start<T>(k);
    return { min, min + TILE_SIZE2, };
}

template<typename T = float>
constexpr Pair<VectorTypeFor<2,T>, VectorTypeFor<2,T>> wall_north(size_t k, float wall_depth)
{
    auto min = tile_start<T>(k) - VectorTypeFor<2,T>{0, wall_depth};
    return { min, min + VectorTypeFor<2,T>{TILE_SIZE2.x(), wall_depth} };
}

template<typename T = float>
constexpr Pair<VectorTypeFor<2,T>, VectorTypeFor<2,T>> wall_west(size_t k, float wall_depth)
{
    auto min = tile_start<T>(k) - VectorTypeFor<2,T>{wall_depth, 0};
    return { min, min + VectorTypeFor<2,T>{wall_depth, TILE_SIZE2.y()} };
}

template<typename T = float>
constexpr Pair<VectorTypeFor<2,T>, VectorTypeFor<2,T>> wall_pillar(size_t k, float wall_depth)
{
    auto min = tile_start<T>(k) - VectorTypeFor<2,T>{wall_depth, 0};
    return { min - VectorTypeFor<2,T>{0, wall_depth}, min + VectorTypeFor<2,T>{wall_depth, TILE_SIZE2.y()} };
}

} // namespace floormat
