#pragma once
#include "tile-constants.hpp"
#include "src/local-coords.hpp"
#include <Corrade/Containers/Pair.h>
#include <Magnum/DimensionTraits.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

template<typename T = float>
constexpr VectorTypeFor<2, T> tile_start(size_t k)
{
    using Vec2 = VectorTypeFor<2,T>;
    constexpr auto half_tile = Vec2{tile_size_xy/2};
    const local_coords coord{k};
    return Vec2(TILE_SIZE2) * Vec2(coord) - half_tile;
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
constexpr Pair<VectorTypeFor<2,T>, VectorTypeFor<2,T>> wall_north(size_t k, T wall_depth)
{
    using Vec2 = VectorTypeFor<2,T>;
    auto min = tile_start<T>(k) - Vec2{0, wall_depth};
    return { min, min + Vec2{tile_size_xy, wall_depth} };
}

template<typename T = float>
constexpr Pair<VectorTypeFor<2,T>, VectorTypeFor<2,T>> wall_west(size_t k, T wall_depth)
{
    using Vec2 = VectorTypeFor<2,T>;
    auto min = tile_start<T>(k) - Vec2{wall_depth, 0};
    return { min, min + Vec2{wall_depth, tile_size_xy} };
}

template<typename T = float>
constexpr Pair<VectorTypeFor<2,T>, VectorTypeFor<2,T>> wall_pillar(size_t k, T wall_depth)
{
    using Vec2 = VectorTypeFor<2,T>;
    auto min = tile_start<T>(k) - Vec2{wall_depth, 0};
    return { min - Vec2{0, wall_depth}, min + Vec2{wall_depth, tile_size_xy} };
}

} // namespace floormat
