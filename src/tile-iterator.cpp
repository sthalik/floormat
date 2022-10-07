#include "tile-iterator.hpp"
#include "tile.hpp"

namespace Magnum::Examples {

template <typename T>
void basic_tile_iterator<T>::swap(basic_tile_iterator<T>& other)
{
    using std::swap;
    swap(ptr, other.ptr);
    swap(pos, other.pos);
}

template <typename T>
template <std::size_t N>
typename std::tuple_element<N, basic_tile_iterator<T>>::type basic_tile_iterator<T>::get()
{
    if constexpr(N == 0)
        return pos == TILE_COUNT ? ptr[0] : ptr[pos];
    else if constexpr(N == 1)
        return pos;
    else if constexpr(N == 2)
        return local_coords{pos};
    else
        return std::void_t<std::integral_constant<int, N>>();
}

template typename std::tuple_element<0, basic_tile_iterator<tile>>::type basic_tile_iterator<tile>::get<0>();
template typename std::tuple_element<1, basic_tile_iterator<tile>>::type basic_tile_iterator<tile>::get<1>();
template typename std::tuple_element<2, basic_tile_iterator<tile>>::type basic_tile_iterator<tile>::get<2>();

template typename std::tuple_element<0, basic_tile_iterator<const tile>>::type basic_tile_iterator<const tile>::get<0>();
template typename std::tuple_element<1, basic_tile_iterator<const tile>>::type basic_tile_iterator<const tile>::get<1>();
template typename std::tuple_element<2, basic_tile_iterator<const tile>>::type basic_tile_iterator<const tile>::get<2>();

template class basic_tile_iterator<tile>;
template class basic_tile_iterator<const tile>;

} // namespace Magnum::Examples
