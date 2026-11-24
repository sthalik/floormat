#pragma once
#include "array-size.hpp"
#include "fwd/tuple-fwd.hpp"
#include <type_traits>
#include <utility>
#include <mg/Math/TypeTraits.h>

namespace floormat::detail::sequence_traits {

template<usize I, typename T> constexpr decltype(auto) adl_get(const T& value) { return get<I>(value); }

template<typename T> struct array_base
{
    static constexpr usize size = array_traits<T>::size;
    template<usize I> static constexpr decltype(auto) get(const T& value) { return array_traits<T>::get(value, I); }
};

} // namespace floormat::detail::sequence_traits

namespace floormat {

template<typename T> struct sequence_traits {};

template<template<typename...> class Tuple> constexpr inline bool is_tuple_template = false;
template<> constexpr inline bool is_tuple_template<std::tuple> = true;
template<> constexpr inline bool is_tuple_template<std::pair> = true;
template<> constexpr inline bool is_tuple_template<Containers::Pair> = true;
template<> constexpr inline bool is_tuple_template<Containers::Triple> = true;

template<template<typename...> class Tuple, typename... Ts>
requires is_tuple_template<Tuple>
struct sequence_traits<Tuple<Ts...>>
{
    static constexpr usize size = sizeof...(Ts);
    template<usize I> static constexpr decltype(auto) get(const Tuple<Ts...>& value)
    {
        return detail::sequence_traits::adl_get<I>(value);
    }
};

template<typename T>
requires Math::IsVector<T>::value
struct sequence_traits<T>
{
    static constexpr usize size = T::Size;
    template<usize I> static constexpr decltype(auto) get(const T& value) { return value[I]; }
};

template<typename T, usize N>
struct sequence_traits<std::array<T, N>> : detail::sequence_traits::array_base<std::array<T, N>> {};

template<usize N, typename T>
struct sequence_traits<StaticArray<N, T>> : detail::sequence_traits::array_base<StaticArray<N, T>> {};

template<typename T, usize N>
struct sequence_traits<T[N]> : detail::sequence_traits::array_base<T[N]> {};

template<typename T>
concept random_access_sequence = requires { sequence_traits<std::remove_cv_t<T>>::size; };

template<random_access_sequence T, usize I>
using sequence_element_t = std::remove_cvref_t<
    decltype(sequence_traits<std::remove_cv_t<T>>::template get<I>(std::declval<const std::remove_cv_t<T>&>()))>;

template<random_access_sequence To, random_access_sequence From, typename Cast>
constexpr To transform_elements(const From& value, Cast cast)
{
    using F = sequence_traits<std::remove_cv_t<From>>;
    static_assert(sequence_traits<std::remove_cv_t<To>>::size == F::size);
    return [&]<usize... I>(std::index_sequence<I...>) {
        return To{cast(std::type_identity<sequence_element_t<To, I>>{}, F::template get<I>(value))...};
    }(std::make_index_sequence<F::size>{});
}

} // namespace floormat
