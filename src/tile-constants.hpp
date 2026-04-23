#pragma once
#include "tile-defs.hpp"
#include <mg/Vector3.h>

namespace floormat {

constexpr inline auto TILE_MAX_DIM20d = Math::Vector3<double>       { TILE_MAX_DIM, TILE_MAX_DIM, 0 };
constexpr inline auto iTILE_SIZE      = Math::Vector3<Int>          { tile_size_xy, tile_size_xy, tile_size_z };
constexpr inline auto iTILE_SIZE2     = Math::Vector2<Int>          { iTILE_SIZE.x(), iTILE_SIZE.y() };
constexpr inline auto TILE_SIZE       = Math::Vector3<float>        { iTILE_SIZE };
constexpr inline auto dTILE_SIZE      = Math::Vector3<double>       { iTILE_SIZE };
constexpr inline auto TILE_SIZE2      = Math::Vector2<float>        { iTILE_SIZE2 };
constexpr inline auto TILE_SIZE20     = Math::Vector3<float>        { (float)iTILE_SIZE.x(), (float)iTILE_SIZE.y(), 0 };

constexpr inline auto most_positive_point    = Vector3i{Vector2i{most_positive_point_xy}, most_positive_point_z};
constexpr inline auto most_negative_point    = Vector3i{Vector2i{most_negative_point_xy}, most_negative_point_z};

} // namespace floormat
