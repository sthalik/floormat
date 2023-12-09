#pragma once
#include "compat/integer-types.hpp"

namespace floormat::Wall {

enum class Group_ : uint8_t { wall, side, top, corner_L, corner_R, COUNT };

enum class Direction_ : uint8_t { N, W, COUNT };

constexpr inline auto Direction_COUNT = (size_t)Wall::Direction_::COUNT;
constexpr inline auto Group_COUNT = (size_t)Wall::Group_::COUNT;

} // namespace floormat::Wall
