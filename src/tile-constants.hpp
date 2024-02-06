#pragma once
#include "tile-defs.hpp"
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

constexpr inline auto TILE_MAX_DIM20d = Magnum::Math::Vector3<double>       { TILE_MAX_DIM, TILE_MAX_DIM, 0 };
constexpr inline auto iTILE_SIZE      = Magnum::Math::Vector3<Int>          { tile_size_xy, tile_size_xy, tile_size_z };
constexpr inline auto iTILE_SIZE2     = Magnum::Math::Vector2<Int>          { iTILE_SIZE.x(), iTILE_SIZE.y() };
constexpr inline auto TILE_SIZE       = Magnum::Math::Vector3<float>        { iTILE_SIZE };
constexpr inline auto dTILE_SIZE      = Magnum::Math::Vector3<double>       { iTILE_SIZE };
constexpr inline auto TILE_SIZE2      = Magnum::Math::Vector2<float>        { iTILE_SIZE2 };
constexpr inline auto TILE_SIZE20     = Magnum::Math::Vector3<float>        { (float)iTILE_SIZE.x(), (float)iTILE_SIZE.y(), 0 };

} // namespace floormat
