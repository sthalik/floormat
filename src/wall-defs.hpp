#pragma once
#include "compat/integer-types.hpp"

namespace floormat::Wall {

enum class Group_ : u8 { wall, side, top, corner, /*pillar,*/ COUNT };

enum class Direction_ : u8 { N, W, COUNT };

constexpr inline auto Direction_COUNT = (usize)Wall::Direction_::COUNT;
constexpr inline auto Group_COUNT = (usize)Wall::Group_::COUNT;

} // namespace floormat::Wall
