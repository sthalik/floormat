#pragma once

#include "local-coords.hpp"
#include "tile.hpp"

#include <iterator>

namespace floormat {

struct tile_iterator_tuple final { // NOLINT(cppcoreguidelines-pro-type-member-init)
    const tile_iterator_tuple* operator->() const noexcept { return this; }
    tile_iterator_tuple* operator->() noexcept { return this; }

    tile_ref x;
    size_t k;
    local_coords pt;
};

struct tile_const_iterator_tuple final { // NOLINT(cppcoreguidelines-pro-type-member-init)
    const tile_const_iterator_tuple* operator->() const noexcept { return this; }
    tile_const_iterator_tuple* operator->() noexcept { return this; }

    tile_proto x;
    size_t k;
    local_coords pt;
};

class tile_iterator final {
    chunk* c;
    size_t pos;

    friend bool operator==(const tile_iterator&, const tile_iterator&) noexcept;

public:
    explicit tile_iterator(chunk& c, size_t pos) noexcept;
    tile_iterator(const tile_iterator&) noexcept;
    tile_iterator& operator=(const tile_iterator&) noexcept;

    void swap(tile_iterator& other) noexcept;

    tile_iterator& operator++() noexcept;
    tile_iterator operator++(int) noexcept;
    tile_iterator_tuple operator->() noexcept;
    tile_iterator_tuple operator*() noexcept;

    using difference_type = ptrdiff_t;
    using value_type = tile_iterator_tuple;
    using pointer = value_type;
    using reference = value_type;
    using iterator_category = std::input_iterator_tag;
};

class tile_const_iterator final {
    const chunk* c;
    size_t pos;

    friend bool operator==(const tile_const_iterator&, const tile_const_iterator&) noexcept;

public:
    explicit tile_const_iterator(const chunk& c, size_t pos) noexcept;
    tile_const_iterator(const tile_const_iterator&) noexcept;
    tile_const_iterator& operator=(const tile_const_iterator&) noexcept;

    void swap(tile_const_iterator& other) noexcept;

    tile_const_iterator& operator++() noexcept;
    tile_const_iterator operator++(int) noexcept;
    tile_const_iterator_tuple operator->() noexcept;
    tile_const_iterator_tuple operator*() noexcept;

    using difference_type = ptrdiff_t;
    using value_type = tile_const_iterator_tuple;
    using pointer = value_type;
    using reference = value_type;
    using iterator_category = std::input_iterator_tag;
};

} // namespace floormat
