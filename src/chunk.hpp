#pragma once
#include "tile.hpp"
#include "tile-iterator.hpp"
#include <type_traits>
#include <array>

namespace Magnum::Examples {

template<typename F, typename Tile>
concept tile_iterator_fn = requires(F fn, Tile& tile) {
    { fn.operator()(tile, std::size_t{}, local_coords{}) } -> std::same_as<void>;
};

template<typename T> class basic_tile_iterator;

struct chunk final
{
    tile& operator[](local_coords xy) { return _tiles[xy.to_index()]; }
    const tile& operator[](local_coords xy) const { return _tiles[xy.to_index()]; }
    tile& operator[](std::size_t i) { return _tiles[i]; }
    const tile& operator[](std::size_t i) const { return _tiles[i]; }
    const auto& tiles() const { return _tiles; }
    auto& tiles() { return _tiles; }

    using iterator = basic_tile_iterator<tile>;
    using const_iterator = basic_tile_iterator<const tile>;

    iterator begin() { return iterator{_tiles.data(), 0}; }
    iterator end() { return iterator{_tiles.data(), _tiles.size()}; }
    const_iterator cbegin() const { return const_iterator{_tiles.data(), 0}; }
    const_iterator cend() const { return const_iterator{_tiles.data(), _tiles.size()}; }
    const_iterator begin() const { return cbegin(); }
    const_iterator end() const { return cend(); }

private:
    std::array<tile, TILE_COUNT> _tiles = {};
};

} // namespace Magnum::Examples
