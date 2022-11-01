#pragma once
#include "tile.hpp"
#include "tile-iterator.hpp"
#include <type_traits>
#include <array>
#include <bitset>

namespace floormat {

template<typename T> class basic_tile_iterator;

struct tile_ref;
struct pass_mode_ref;

struct chunk final
{
    friend struct tile_ref;
    friend struct pass_mode_ref;

#if 0
    tile& operator[](local_coords xy) noexcept { return _tiles[xy.to_index()]; }
    const tile& operator[](local_coords xy) const noexcept { return _tiles[xy.to_index()]; }
    tile& operator[](std::size_t i) noexcept { return _tiles[i]; }
    const tile& operator[](std::size_t i) const noexcept { return _tiles[i]; }
    const auto& tiles() const noexcept { return _tiles; }
    auto& tiles() noexcept { return _tiles; }
#endif

    using iterator = basic_tile_iterator<tile>;
    using const_iterator = basic_tile_iterator<const tile>;

#if 0
    iterator begin() noexcept { return iterator{_tiles.data(), 0}; }
    iterator end() noexcept { return iterator{_tiles.data(), _tiles.size()}; }
    const_iterator cbegin() const noexcept { return const_iterator{_tiles.data(), 0}; }
    const_iterator cend() const noexcept { return const_iterator{_tiles.data(), _tiles.size()}; }
    const_iterator begin() const noexcept { return cbegin(); }
    const_iterator end() const noexcept { return cend(); }
#endif

    bool empty(bool force = false) const noexcept;

    chunk() noexcept = default;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(chunk);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(chunk);

private:
    static constexpr std::size_t PASS_BITS = 2;

    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT> _ground_atlases, _wall_north_atlases, _wall_west_atlases;
    std::array<std::uint16_t, TILE_COUNT> _ground_variants, _wall_north_variants, _wall_west_variants;
    std::bitset<TILE_COUNT*2> _passability;
    mutable bool _maybe_empty = true;
};

} // namespace floormat
