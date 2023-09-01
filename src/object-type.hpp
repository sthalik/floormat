#pragma once

namespace floormat {

enum class object_type : unsigned char {
    none, critter, scenery, light,
};
constexpr inline size_t object_type_BITS = 3;

} // namespace floormat
