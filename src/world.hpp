#pragma once
#include "src/chunk.hpp"

namespace Magnum::Examples {

struct chunk_coords final {
    std::int16_t x = 0, y = 0;
    static constexpr std::size_t max_bits = sizeof(chunk_coords::x)*8 * 3 / 4;
    static_assert(max_bits*4/3/8 == sizeof(decltype(chunk_coords::x)));
};

struct global_coords final {
    chunk_coords chunk;
    local_coords tile;
};

} // namespace Magnum::Examples
