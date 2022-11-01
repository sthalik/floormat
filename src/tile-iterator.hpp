#pragma once

#include "compat/integer-types.hpp"
#include "local-coords.hpp"
#include "tile.hpp"
#include <compare>

namespace floormat {

struct tile_iterator_tuple final { // NOLINT(cppcoreguidelines-pro-type-member-init)
    const tile_iterator_tuple* operator->() const noexcept { return this; }
    tile_iterator_tuple* operator->() noexcept { return this; }

    tile_ref tile;
    std::size_t i;
    local_coords pos;
};

class tile_iterator final {
    chunk* c;
    std::size_t pos;

public:
    explicit tile_iterator(chunk& c, std::size_t pos) noexcept;
    tile_iterator(const tile_iterator&) noexcept;
    tile_iterator& operator=(const tile_iterator&) noexcept;

    std::strong_ordering operator<=>(const tile_iterator&) const noexcept;
    void swap(tile_iterator& other) noexcept;

    tile_iterator& operator++() noexcept;
    tile_iterator operator++(int) noexcept;
    tile_iterator_tuple operator->() noexcept;
    tile_iterator_tuple operator*() noexcept;
};

} // namespace floormat
