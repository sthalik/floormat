#pragma once

namespace floormat {

enum class collision : unsigned char {
    view, shoot, move,
};

enum class collision_type : unsigned char {
    none, object, scenery, geometry,
};

constexpr inline size_t collision_data_BITS = 60;

struct collision_data final {
    uint64_t tag       : 2;
    uint64_t pass      : 2;
    uint64_t data      : collision_data_BITS;
};

} // namespace floormat