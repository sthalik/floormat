#pragma once

#include "local-coords.hpp"
#include <iterator>
#include <tuple>
#include <utility>
#include <type_traits>

namespace floormat {

struct tile;
template<typename T> class basic_tile_iterator;
template<typename T> struct tile_tuple;

} // namespace floormat

namespace std {

template<typename T> struct tuple_size<floormat::tile_tuple<T>> : std::integral_constant<std::size_t, 3> {};

template<> struct tuple_element<0, floormat::tile_tuple<floormat::tile>> { using type = floormat::tile&; };
template<> struct tuple_element<0, floormat::tile_tuple<const floormat::tile>> { using type = const floormat::tile&; };
template<> struct tuple_element<0, const floormat::tile_tuple<floormat::tile>> { using type = const floormat::tile&; };

template<typename T> struct tuple_element<1, floormat::tile_tuple<T>> { using type = std::size_t; };
template<typename T> struct tuple_element<2, floormat::tile_tuple<T>> { using type = floormat::local_coords; };

} // namespace std

namespace floormat {

template<typename T>
struct tile_tuple {
    tile_tuple(T* ptr, std::size_t pos) : data{ptr, pos} {}
    tile_tuple(const tile_tuple<T>&) = default;
    tile_tuple<T>& operator=(const tile_tuple<T>&) = default;

    template <std::size_t N>
    typename std::tuple_element<N, tile_tuple<T>>::type get()
    {
        auto& [ptr, pos] = data;
        if constexpr(N == 0)
            return ptr[pos];
        else if constexpr(N == 1)
            return pos;
        else if constexpr(N == 2)
            return local_coords{pos};
        else
            return std::void_t<std::integral_constant<int, N>>();
    }

    template <std::size_t N>
    typename std::tuple_element<N, const tile_tuple<const T>>::type get() const {
        return const_cast<tile_tuple<T>&>(*this).get<N>();
    }

    auto operator<=>(const tile_tuple<T>&) const noexcept = default;

protected:
    std::tuple<T*, std::size_t> data = {nullptr, 0};
};

} // namespace floormat

namespace floormat {

template<typename T>
class basic_tile_iterator final : private tile_tuple<T> {
public:
    explicit basic_tile_iterator(T* ptr, std::size_t pos) noexcept : tile_tuple<T>{ptr, pos} {}
    basic_tile_iterator(const basic_tile_iterator&) = default;
    basic_tile_iterator<T>& operator=(const basic_tile_iterator<T>&) = default;

    auto operator<=>(const basic_tile_iterator<T>&) const noexcept = default;
    void swap(basic_tile_iterator<T>& other) { std::swap(this->data, other.data); }

    basic_tile_iterator<T>& operator++() { auto& [ptr, pos] = this->data; pos++; return *this; }
    basic_tile_iterator<T>  operator++(int) { auto tmp = *this; operator++(); return tmp; }
    tile_tuple<T>* operator->() { return this; }
    tile_tuple<T>& operator*() { return *this; }
};

extern template class basic_tile_iterator<tile>;
extern template class basic_tile_iterator<const tile>;

} // namespace floormat

namespace std {

template<typename Tile>
class iterator_traits<floormat::basic_tile_iterator<Tile>> {
    using T = floormat::basic_tile_iterator<Tile>;
public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using reference = T&;
    using pointer = T*;
    using iterator_category = std::input_iterator_tag;  //usually std::forward_iterator_tag or similar
};

} // namespace std
