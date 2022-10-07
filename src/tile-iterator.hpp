#pragma once

#include "local-coords.hpp"
#include "tile.hpp"
#include <iterator>
#include <tuple>
#include <utility>
#include <type_traits>

namespace Magnum::Examples {

template<typename T>
class basic_tile_iterator;

struct tile;

} // namespace Magnum::Examples

namespace std {

template<typename T>
constexpr void swap(Magnum::Examples::basic_tile_iterator<T>& lhs,
                    Magnum::Examples::basic_tile_iterator<T>& rhs) noexcept;

} // namespace std

namespace Magnum::Examples {

namespace detail {

template<typename T>
class tile_tuple_wrapper final {
    using value_type = std::tuple<T&, std::size_t, local_coords>;
    value_type value;

public:
    constexpr tile_tuple_wrapper(value_type x) : value(x) {}
    constexpr value_type& operator*() noexcept { return value; }
    constexpr value_type* operator->() noexcept { return &value; }
};

} // namespace detail

template<typename T>
class basic_tile_iterator final {
    T* ptr;
    std::size_t pos = 0;

    friend constexpr void swap(basic_tile_iterator<T>& lhs, basic_tile_iterator<T>& rhs) noexcept;

public:
    using value_type = std::tuple<T&, std::size_t, local_coords>;

    explicit constexpr basic_tile_iterator(T* ptr, std::size_t pos) noexcept : ptr(ptr), pos(pos) {}
    constexpr ~basic_tile_iterator() noexcept = default;

    constexpr basic_tile_iterator<T>& operator=(const basic_tile_iterator<T>&) noexcept = default;
    constexpr basic_tile_iterator<T>& operator++() noexcept { pos++; return *this; }
    constexpr basic_tile_iterator<T> operator++(int) noexcept { auto tmp = *this; operator++(); return tmp; }
    constexpr value_type operator*() const noexcept { // NOLINT(bugprone-exception-escape)
        ASSERT(pos < TILE_COUNT);
        return {ptr[pos], pos, local_coords{pos}};
    }
    constexpr detail::tile_tuple_wrapper<T> operator->() const noexcept { // NOLINT(bugprone-exception-escape)
        return {operator*()};
    }
    constexpr auto operator<=>(const basic_tile_iterator<T>&) const noexcept = default;
};

template<typename T>
constexpr void swap(basic_tile_iterator<T>& lhs, basic_tile_iterator<T>& rhs) noexcept
{
    using std::swap;
    swap(lhs.ptr, rhs.ptr);
    swap(lhs.pos, rhs.ptr);
}

extern template class basic_tile_iterator<tile>;
extern template class basic_tile_iterator<const tile>;

} // namespace Magnum::Examples

namespace std {

template<typename Tile>
class iterator_traits<Magnum::Examples::basic_tile_iterator<Tile>> {
    using T = typename Magnum::Examples::basic_tile_iterator<Tile>::value_type;
public:
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using reference = T&;
    using pointer = T*;
    using iterator_category = std::input_iterator_tag;  //usually std::forward_iterator_tag or similar
};

} // namespace std
