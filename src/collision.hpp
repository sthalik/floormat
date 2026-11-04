#pragma once

namespace floormat {

enum class collision_type : unsigned char {
    none, object, scenery, geometry,
};

constexpr inline usize collision_data_BITS = 60;

struct collision_data final {
    u64 type : 2;
    u64 pass : 2;
    u64 id   : collision_data_BITS;

    bool operator==(const collision_data&) const noexcept;
};

} // namespace floormat
