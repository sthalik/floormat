#include "tile-iterator.hpp"
#include "tile.hpp"

namespace floormat {

basic_tile_iterator::basic_tile_iterator(chunk& c, std::size_t pos) noexcept :
    c{&c}, pos{pos}
{
}

basic_tile_iterator::basic_tile_iterator(const basic_tile_iterator&) noexcept = default;
basic_tile_iterator& basic_tile_iterator::operator=(const basic_tile_iterator&) noexcept = default;
std::strong_ordering basic_tile_iterator::operator<=>(const basic_tile_iterator&) const noexcept = default;
void basic_tile_iterator::swap(basic_tile_iterator& other) { std::swap(c, other.c); std::swap(pos, other.pos); }


} // namespace floormat
