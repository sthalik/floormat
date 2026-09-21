#pragma once
#include <mg/Math/TypeTraits.h>
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

// Size is inherited from Math::Vector, and partial specialization doesn't see through inheritance.
template<typename T> requires (Math::IsVector<T>::value)
struct array_size_<T> : std::integral_constant<size_t, T::Size> {};

template<typename T, typename U> struct array_rebind_ { using type = std::array<U, array_size_<T>::value>; };
template<size_t N, typename T, typename U> struct array_rebind_<StaticArray<N, T>, U> { using type = StaticArray<N, U>; };

template<template<class> class C, typename T, typename U>
requires (Math::IsVector<C<T>>::value && Math::IsScalar<U>::value)
struct array_rebind_<C<T>, U> { using type = C<U>; };

template<template<size_t, class> class C, size_t N, typename T, typename U>
requires (Math::IsVector<C<N, T>>::value && Math::IsScalar<U>::value)
struct array_rebind_<C<N, T>, U> { using type = C<N, U>; };

template<typename T> struct array_traits_base
{
    static constexpr size_t size = array_size_<T>::value;
    template<typename U> using rebind = typename array_rebind_<T, U>::type;
    static constexpr decltype(auto) get(const T& array, size_t index) { return array.data()[index]; }
};

} // namespace floormat::detail

namespace floormat {

template<typename T> struct array_traits : detail::array_traits_base<T>
{
    template<typename... Us> static constexpr T make(Us&&... elements) { return T{::floormat::forward<Us>(elements)...}; }
};

template<typename T, size_t N> struct array_traits<std::array<T, N>> : detail::array_traits_base<std::array<T, N>>
{
    // std::array wraps a single T[N] member, so the inner braces are the array's
    template<typename... Us> static constexpr std::array<T, N> make(Us&&... elements) { return std::array<T, N>{{ ::floormat::forward<Us>(elements)... }}; }
};

template<size_t N, typename T> struct array_traits<StaticArray<N, T>> : detail::array_traits_base<StaticArray<N, T>>
{
    template<typename... Us> static constexpr StaticArray<N, T> make(Us&&... elements) { return StaticArray<N, T>{InPlaceInit, ::floormat::forward<Us>(elements)...}; }
};

// a C array has no .data() and can't be returned
template<typename T, size_t N> struct array_traits<T[N]>
{
    static constexpr size_t size = N;
    template<typename U> using rebind = std::array<U, N>;
    static constexpr const T& get(const T (&array)[N], size_t index) { return array[index]; }
};

template<typename T> constexpr inline size_t static_array_size = array_traits<std::remove_cvref_t<T>>::size;
template<typename T> constexpr inline size_t array_size(const T&) noexcept { return static_array_size<T>; }

} // namespace floormat
