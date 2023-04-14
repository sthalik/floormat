#pragma once
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>

namespace floormat {

constexpr inline unsigned char TILE_MAX_DIM = 16;
constexpr inline size_t TILE_COUNT = size_t{TILE_MAX_DIM}*size_t{TILE_MAX_DIM};

constexpr inline auto TILE_MAX_DIM20d = Magnum::Math::Vector3<double>       { TILE_MAX_DIM, TILE_MAX_DIM, 0 };
constexpr inline auto iTILE_SIZE      = Magnum::Math::Vector3<Int>          { 64, 64, 128 };
constexpr inline auto uiTILE_SIZE     = Magnum::Math::Vector3<UnsignedInt>  { iTILE_SIZE };
constexpr inline auto bTILE_SIZE      = Magnum::Math::Vector3<Byte>         { iTILE_SIZE };
constexpr inline auto iTILE_SIZE2     = Magnum::Math::Vector2<Int>          { iTILE_SIZE[0], iTILE_SIZE[1] };
constexpr inline auto TILE_SIZE       = Magnum::Math::Vector3<float>        { iTILE_SIZE };
constexpr inline auto dTILE_SIZE      = Magnum::Math::Vector3<double>       { iTILE_SIZE };
constexpr inline auto TILE_SIZE2      = Magnum::Math::Vector2<float>        { iTILE_SIZE2 };
constexpr inline auto TILE_SIZE20     = Magnum::Math::Vector3<float>        { (float)iTILE_SIZE[0], (float)iTILE_SIZE[1], 0 };

} // namespace floormat
