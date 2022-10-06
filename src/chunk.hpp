#pragma once
#include "tile.hpp"
#include <type_traits>
#include <array>

namespace Magnum::Examples {

struct chunk final
{
    constexpr tile& operator[](local_coords xy) { return _tiles[xy.to_index()]; }
    constexpr const tile& operator[](local_coords xy) const { return _tiles[xy.to_index()]; }
    constexpr tile& operator[](std::size_t i) { return _tiles[i]; }
    constexpr const tile& operator[](std::size_t i) const { return _tiles[i]; }
    auto& tiles() const { return _tiles; }

    template<typename F>
    requires std::invocable<F, tile&, std::size_t, local_coords>
    constexpr inline void foreach_tile(F&& fun) { foreach_tile_<F, chunk&>(std::forward<F>(fun)); }

    template<typename F>
    requires std::invocable<F, const tile&, std::size_t, local_coords>
    constexpr inline void foreach_tile(F&& fun) const { foreach_tile_<F, const chunk&>(std::forward<F>(fun)); }

private:
    template<typename F, typename Self>
    constexpr void foreach_tile_(F&& fun);

    std::array<tile, TILE_COUNT> _tiles = {};
};

template<typename F, typename Self>
constexpr void chunk::foreach_tile_(F&& fun)
{
    constexpr auto N = TILE_MAX_DIM;
    std::size_t k = 0;
    for (std::size_t j = 0; j < N; j++)
        for (std::size_t i = 0; i < N; i++, k++)
            fun(const_cast<Self>(*this)._tiles[k], k,
                local_coords{(std::uint8_t)i, (std::uint8_t)j});
}

#if 0
struct chunk_coords final {
    std::int16_t x = 0, y = 0;
    constexpr std::size_t to_index() const noexcept;

    static constexpr std::size_t max_bits = sizeof(chunk_coords::x)*8 * 3 / 4;
    static_assert(max_bits*4/3/8 == sizeof(decltype(chunk_coords::x)));
};
#endif

#if 0
struct global_coords final {
    std::uint32_t x = 0, y = 0;
    constexpr global_coords() noexcept = default;
    constexpr global_coords(decltype(x) x, decltype(y) y) noexcept : x{x}, y{y} {}
    constexpr global_coords(chunk_coords c, local_coords tile) noexcept;
};
#endif

} // namespace Magnum::Examples
