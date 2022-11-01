#include "tile-iterator.hpp"
#include "tile.hpp"

namespace floormat {

tile_iterator::tile_iterator(chunk& c, std::size_t pos) noexcept : c{&c}, pos{pos} {}
tile_iterator::tile_iterator(const tile_iterator&) noexcept = default;
tile_iterator& tile_iterator::operator=(const tile_iterator&) noexcept = default;

tile_iterator& tile_iterator::operator++() noexcept { pos++; return *this; }
tile_iterator tile_iterator::operator++() noexcept { auto it = *this; pos++; return it; }
void tile_iterator::swap(tile_iterator& other) noexcept { std::swap(c, other.c); std::swap(pos, other.pos); }

std::strong_ordering tile_iterator::operator<=>(const tile_iterator&) const noexcept = default;

tile_iterator_tuple tile_iterator::operator->() { return { tile_ref{*c, i}, i, local_coords{i} }; }
tile_iterator_tuple tile_iterator::operator*()  { return { tile_ref{*c, i}, i, local_coords{i} }; }

} // namespace floormat
