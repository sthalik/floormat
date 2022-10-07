#pragma once
#include "tile.hpp"
#include <type_traits>
#include <array>

namespace Magnum::Examples {

template<typename F, typename Tile>
concept tile_iterator = requires(F fn, Tile& tile) {
    { fn.operator()(tile, std::size_t{}, local_coords{}) } -> std::same_as<void>;
};

struct chunk final
{
    constexpr tile& operator[](local_coords xy) { return _tiles[xy.to_index()]; }
    constexpr const tile& operator[](local_coords xy) const { return _tiles[xy.to_index()]; }
    constexpr tile& operator[](std::size_t i) { return _tiles[i]; }
    constexpr const tile& operator[](std::size_t i) const { return _tiles[i]; }
    const auto& tiles() const { return _tiles; }
    auto& tiles() { return _tiles; }

    template<tile_iterator<tile&> F>
    constexpr inline void foreach_tile(F&& fun) {
        foreach_tile_<F, chunk&>(std::forward<F>(fun));
    }

    template<tile_iterator<const tile&> F>
    constexpr inline void foreach_const_tile(F&& fun) const {
        const_cast<chunk*>(this)->foreach_tile_<F, const chunk&>(std::forward<F>(fun));
    }

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

} // namespace Magnum::Examples
