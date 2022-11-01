#include "tile-iterator.hpp"
#include "tile.hpp"

namespace floormat {

tile_iterator::tile_iterator(chunk& c, std::size_t pos) noexcept : c{&c}, pos{pos} {}
tile_iterator::tile_iterator(const tile_iterator&) noexcept = default;
tile_iterator& tile_iterator::operator=(const tile_iterator&) noexcept = default;

tile_iterator& tile_iterator::operator++() noexcept { pos++; return *this; }
tile_iterator tile_iterator::operator++(int) noexcept { auto it = *this; pos++; return it; }
void tile_iterator::swap(tile_iterator& other) noexcept { std::swap(c, other.c); std::swap(pos, other.pos); }

bool operator==(const tile_iterator& a, const tile_iterator& b) noexcept { return a.c == b.c && a.pos == b.pos; }

tile_iterator_tuple tile_iterator::operator->() noexcept { return { tile_ref{*c, std::uint8_t(pos)}, pos, local_coords{pos} }; }
tile_iterator_tuple tile_iterator::operator*()  noexcept { return { tile_ref{*c, std::uint8_t(pos)}, pos, local_coords{pos} }; }

} // namespace floormat
