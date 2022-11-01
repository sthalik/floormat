#pragma once
#include "tile.hpp"
#include "tile-iterator.hpp"
#include <type_traits>
#include <array>
#include <bitset>

namespace floormat {

struct chunk final
{
    friend struct tile_ref;
    friend struct pass_mode_ref;

    tile_ref operator[](std::size_t idx) noexcept { return { *this, std::uint8_t(idx) }; }
    tile_proto operator[](std::size_t idx) const noexcept { return tile_proto(tile_ref { *const_cast<chunk*>(this), std::uint8_t(idx) }); }
    tile_ref operator[](local_coords xy) noexcept { return operator[](xy.to_index()); }
    tile_proto operator[](local_coords xy) const noexcept { return operator[](xy.to_index()); }

    using iterator = tile_iterator;

    iterator begin() noexcept { return iterator { *this, 0 }; }
    iterator end() noexcept { return iterator { *this, TILE_COUNT }; }

    bool empty(bool force = false) const noexcept;

    chunk() noexcept = default;

    fm_DECLARE_DELETED_COPY_ASSIGNMENT(chunk);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(chunk);

private:
    static constexpr std::size_t PASS_BITS = 2;

    std::array<std::shared_ptr<tile_atlas>, TILE_COUNT> _ground_atlases, _wall_north_atlases, _wall_west_atlases;
    std::array<std::uint16_t, TILE_COUNT> _ground_variants = {}, _wall_north_variants = {}, _wall_west_variants = {};
    std::bitset<TILE_COUNT*2> _passability = {};
    mutable bool _maybe_empty = true;
};

} // namespace floormat
