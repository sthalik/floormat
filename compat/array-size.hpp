#pragma once
#include <array>

namespace floormat::detail {

template<typename T> struct array_size_;
template<typename T, size_t N> struct array_size_<T(&)[N]> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<T(*)[N]> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<T[N]> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<std::array<T, N>> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<StaticArray<N, T>> : std::integral_constant<size_t, N> {};

template<typename C, typename T> struct array_size_<T C::*> : std::integral_constant<size_t, array_size_<std::remove_cvref_t<T>>::value> {};
//template<typename T, typename U, size_t N> struct array_size_< T(U::*)[N] > : std::integral_constant<size_t, N> {}; // should be redundant

} // namespace floormat::detail

namespace floormat {

template<typename T> constexpr inline size_t static_array_size = detail::array_size_<std::remove_cvref_t<T>>::value;
template<typename T> constexpr inline size_t array_size(const T&) noexcept { return detail::array_size_<std::remove_cvref_t<T>>::value; }

} // namespace floormat
