#pragma once
#include "src/chunk.hpp"
#include "compat/assert.hpp"

namespace floormat {

struct chunk_coords final {
    std::int16_t x = 0, y = 0;
};

struct global_coords final {
    std::int16_t cx = 0, cy = 0;
    std::int32_t x = 0, y = 0;

    constexpr global_coords(chunk_coords c, local_coords xy)
        : cx{c.x}, cy{c.y}, x{xy.x}, y{xy.y}
    {}
};

} // namespace floormat
