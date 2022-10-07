#pragma once

#include "local-coords.hpp"
#include <iterator>
#include <utility>
#include <type_traits>

namespace Magnum::Examples {

template<typename T> class basic_tile_iterator;
struct tile;

} // namespace Magnum::Examples

namespace std {

template<typename T>
void swap(Magnum::Examples::basic_tile_iterator<T>& lhs,
          Magnum::Examples::basic_tile_iterator<T>& rhs) noexcept;

template<typename T> struct tuple_size<Magnum::Examples::basic_tile_iterator<T>> : std::integral_constant<std::size_t, 3> {};
template<typename T> struct tuple_element<0, Magnum::Examples::basic_tile_iterator<T>> { using type = T&; };
template<typename T> struct tuple_element<1, Magnum::Examples::basic_tile_iterator<T>> { using type = std::size_t; };
template<typename T> struct tuple_element<2, Magnum::Examples::basic_tile_iterator<T>> { using type = Magnum::Examples::local_coords; };

} // namespace std

namespace Magnum::Examples {

template<typename T>
class basic_tile_iterator final {
    T* ptr;
    std::size_t pos = 0;

public:
    using value_type = std::tuple<T&, std::size_t, local_coords>;

    explicit basic_tile_iterator(T* ptr, std::size_t pos) noexcept : ptr(ptr), pos(pos) {}
    ~basic_tile_iterator() noexcept = default;

    basic_tile_iterator<T>& operator=(const basic_tile_iterator<T>&) = default;
    basic_tile_iterator<T>& operator++() { pos++; return *this; }
    basic_tile_iterator<T>  operator++(int) { auto tmp = *this; operator++(); return tmp; }
    basic_tile_iterator<T>* operator->() { return this; }
    basic_tile_iterator<T>& operator*() { return *this; }
    auto operator<=>(const basic_tile_iterator<T>&) const noexcept = default;
    void swap(basic_tile_iterator<T>& other);
    template<std::size_t N>
    typename std::tuple_element<N, basic_tile_iterator<T>>::type get();
};

extern template class basic_tile_iterator<tile>;
extern template class basic_tile_iterator<const tile>;

extern template typename std::tuple_element<0, basic_tile_iterator<tile>>::type basic_tile_iterator<tile>::get<0>();
extern template typename std::tuple_element<1, basic_tile_iterator<tile>>::type basic_tile_iterator<tile>::get<1>();
extern template typename std::tuple_element<2, basic_tile_iterator<tile>>::type basic_tile_iterator<tile>::get<2>();

extern template typename std::tuple_element<0, basic_tile_iterator<const tile>>::type basic_tile_iterator<const tile>::get<0>();
extern template typename std::tuple_element<1, basic_tile_iterator<const tile>>::type basic_tile_iterator<const tile>::get<1>();
extern template typename std::tuple_element<2, basic_tile_iterator<const tile>>::type basic_tile_iterator<const tile>::get<2>();

} // namespace Magnum::Examples

namespace std {

template<typename Tile>
class iterator_traits<Magnum::Examples::basic_tile_iterator<Tile>> {
    using T = Magnum::Examples::basic_tile_iterator<Tile>;
public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using reference = T&;
    using pointer = T*;
    using iterator_category = std::input_iterator_tag;  //usually std::forward_iterator_tag or similar
};

} // namespace std
