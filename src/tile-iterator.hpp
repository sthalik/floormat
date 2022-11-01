#pragma once

#include "local-coords.hpp"
#include "tile.hpp"
#include <iterator>
#include <tuple>
#include <utility>
#include <type_traits>

namespace floormat {

struct tile_tuple final {
    const tile_tuple* operator->() const noexcept { return this; }
    tile_tuple* operator->() noexcept { return this; }

    tile_ref tile;
    std::size_t i;
    local_coords pos;
};

class basic_tile_iterator final {
    chunk* c;
    std::size_t pos;

public:
    explicit basic_tile_iterator(chunk& c, std::size_t pos) noexcept;
    basic_tile_iterator(const basic_tile_iterator&) noexcept;
    basic_tile_iterator& operator=(const basic_tile_iterator&) noexcept;

    std::strong_ordering operator<=>(const basic_tile_iterator&) const noexcept;
    void swap(basic_tile_iterator& other);

    basic_tile_iterator& operator++();
    basic_tile_iterator operator++(int);
    tile_tuple operator->();
    tile_tuple operator*();
};

} // namespace floormat

