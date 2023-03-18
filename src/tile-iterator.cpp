#include "tile-iterator.hpp"
#include "tile.hpp"

namespace floormat {

tile_iterator::tile_iterator(chunk& c, size_t pos) noexcept : c{&c}, pos{pos} {}
tile_iterator::tile_iterator(const tile_iterator&) noexcept = default;
tile_iterator& tile_iterator::operator=(const tile_iterator&) noexcept = default;
tile_iterator& tile_iterator::operator++() noexcept { pos++; return *this; }
tile_iterator tile_iterator::operator++(int) noexcept { auto it = *this; pos++; return it; }
void tile_iterator::swap(tile_iterator& other) noexcept { std::swap(c, other.c); std::swap(pos, other.pos); }
bool operator==(const tile_iterator& a, const tile_iterator& b) noexcept { return a.c == b.c && a.pos == b.pos; }
tile_iterator_tuple tile_iterator::operator->() noexcept { return { tile_ref{*c, uint8_t(pos)}, pos, local_coords{pos} }; }
tile_iterator_tuple tile_iterator::operator*()  noexcept { return operator->(); }

tile_const_iterator::tile_const_iterator(const chunk& c, size_t pos) noexcept : c{&c}, pos{pos} {}
tile_const_iterator::tile_const_iterator(const tile_const_iterator& x) noexcept = default;
tile_const_iterator& tile_const_iterator::operator=(const tile_const_iterator& x) noexcept { if (this != &x) { c = x.c; pos = x.pos; } return *this; }
tile_const_iterator& tile_const_iterator::operator++() noexcept { pos++; return *this; }
tile_const_iterator tile_const_iterator::operator++(int) noexcept { auto it = *this; pos++; return it; }
void tile_const_iterator::swap(tile_const_iterator& other) noexcept { std::swap(c, other.c); std::swap(pos, other.pos); }
bool operator==(const tile_const_iterator& a, const tile_const_iterator& b) noexcept { return a.c == b.c && a.pos == b.pos; }
tile_const_iterator_tuple tile_const_iterator::operator->() noexcept { return { tile_proto(tile_ref{*const_cast<chunk*>(c), uint8_t(pos)}), pos, local_coords{pos}, }; }
tile_const_iterator_tuple tile_const_iterator::operator*() noexcept { return operator->(); }

} // namespace floormat
