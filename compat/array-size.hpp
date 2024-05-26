#pragma once
#include <array>

namespace floormat::detail {

template<typename T> struct array_size_;
template<typename T, size_t N> struct array_size_<T(&)[N]> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<T[N]> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<std::array<T, N>> : std::integral_constant<size_t, N> {};
template<typename T, size_t N> struct array_size_<StaticArray<N, T>> : std::integral_constant<size_t, N> {};

} // namespace floormat::detail

namespace floormat {

template<typename T> constexpr inline size_t static_array_size = detail::array_size_<T>::value;
template<typename T> constexpr inline size_t array_size(const T&) noexcept { return detail::array_size_<T>::value; }

} // namespace floormat
