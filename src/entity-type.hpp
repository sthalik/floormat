#pragma once

namespace floormat {

enum class entity_type : unsigned char {
    none, character, scenery,
};
constexpr inline size_t entity_type_BITS = 3;

} // namespace floormat
