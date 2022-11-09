#pragma once
#include "tile.hpp"
#include "tile-iterator.hpp"
#include "scenery.hpp"
#include <type_traits>
#include <array>
#include <bitset>

namespace floormat {

struct anim_atlas;

struct chunk final
{
    friend struct tile_ref;
    friend struct pass_mode_ref;

    tile_ref operator[](std::size_t idx) noexcept;
    tile_proto operator[](std::size_t idx) const noexcept;
    tile_ref operator[](local_coords xy) noexcept;
    tile_proto operator[](local_coords xy) const noexcept;

    using iterator = tile_iterator;
    using const_iterator = tile_const_iterator;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

    bool empty(bool force = false) const noexcept;

    chunk() noexcept;
    chunk(const chunk&) = delete;
    chunk& operator=(const chunk&) = delete;
    chunk(chunk&&) noexcept;
    chunk& operator=(chunk&&) noexcept;

private:
    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT> _ground_atlases, _wall_north_atlases, _wall_west_atlases;
    std::array<std::shared_ptr<anim_atlas>, TILE_COUNT> _scenery_atlases;
    std::array<scenery, TILE_COUNT> _scenery_variants = {};
    std::array<variant_t, TILE_COUNT> _ground_variants = {}, _wall_north_variants = {}, _wall_west_variants = {};
    std::bitset<TILE_COUNT*2> _passability = {};
    mutable bool _maybe_empty = true;
};

} // namespace floormat
