#pragma once
#include "src/chunk.hpp"
#include "compat/assert.hpp"

namespace Magnum::Examples {

struct chunk_coords final {
    std::int16_t x = 0, y = 0;
#if 0
    static_assert(std::is_same_v<decltype(x), decltype(x)>);
    static constexpr std::size_t max_bits = sizeof(chunk_coords::x)*8 * 3 / 4;
    static_assert(max_bits*4/3/8 == sizeof(decltype(chunk_coords::x)));
    constexpr chunk_coords(std::int16_t x, std::int16_t y);
#endif
};

#if 0
constexpr chunk_coords::chunk_coords(std::int16_t x, std::int16_t y) : x{x}, y{y} {
    using s = std::make_signed_t<std::size_t>;
}
#endif

struct global_coords final {
    chunk_coords chunk;
    local_coords tile;
};

} // namespace Magnum::Examples
