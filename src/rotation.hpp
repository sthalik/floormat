#pragma once

namespace floormat {

enum class rotation : unsigned char {
    N, NE, E, SE, S, SW, W, NW,
};

constexpr inline usize rotation_BITS = 3;
constexpr inline usize rotation_MASK = (1 << rotation_BITS)-1;
constexpr inline rotation rotation_COUNT = rotation{1 << rotation_BITS};

} // namespace floormat
