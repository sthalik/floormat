#pragma once
#include "tile.hpp"
#include "tile-iterator.hpp"
#include <type_traits>
#include <array>

namespace floormat {

template<typename T> class basic_tile_iterator;

struct chunk final
{
    tile& operator[](local_coords xy) noexcept { return _tiles[xy.to_index()]; }
    const tile& operator[](local_coords xy) const noexcept { return _tiles[xy.to_index()]; }
    tile& operator[](std::size_t i) noexcept { return _tiles[i]; }
    const tile& operator[](std::size_t i) const noexcept { return _tiles[i]; }
    const auto& tiles() const noexcept { return _tiles; }
    auto& tiles() noexcept { return _tiles; }

    using iterator = basic_tile_iterator<tile>;
    using const_iterator = basic_tile_iterator<const tile>;

    iterator begin() noexcept { return iterator{_tiles.data(), 0}; }
    iterator end() noexcept { return iterator{_tiles.data(), _tiles.size()}; }
    const_iterator cbegin() const noexcept { return const_iterator{_tiles.data(), 0}; }
    const_iterator cend() const noexcept { return const_iterator{_tiles.data(), _tiles.size()}; }
    const_iterator begin() const noexcept { return cbegin(); }
    const_iterator end() const noexcept { return cend(); }

    bool empty(bool force = false) const noexcept;

    chunk() noexcept = default;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(chunk);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(chunk);

private:
    std::array<tile, TILE_COUNT> _tiles = {};
    mutable bool _maybe_empty = true;
};

} // namespace floormat
