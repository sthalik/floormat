#pragma once
#include "name-of.hpp"
#include <array>
#include <tuple>
#include <type_traits>
#include <utility>

namespace floormat::detail::sort_by_name_impl {

consteval bool is_name_portable(StringView name)
{
    for (auto i = 0u; i < (u32)name.size(); i++)
        if (name[i] == '<' || name[i] == '(' || name[i] == '{' || name[i] == '`')
            return false;
    return true;
}

} // namespace floormat::detail::sort_by_name_impl

namespace floormat {

template<typename T>
constexpr inline bool is_name_portable =
    detail::sort_by_name_impl::is_name_portable(name_of<std::remove_const_t<T>>);

} // namespace floormat

namespace floormat::detail::sort_by_name_impl {

consteval bool is_less(StringView a, StringView b)
{
    const auto n = (u32)(a.size() < b.size() ? a.size() : b.size());
    for (auto i = 0u; i < n; i++)
        if (a[i] != b[i])
            return (unsigned char)a[i] < (unsigned char)b[i];
    return a.size() < b.size();
}

template<typename... Ts>
consteval std::array<u32, sizeof...(Ts)> sorted_indices()
{
    constexpr auto N = (u32)sizeof...(Ts);
    constexpr std::array<StringView, N> names = { name_of<std::remove_const_t<Ts>>... };
    std::array<u32, N> order = {};
    for (auto i = 0u; i < N; i++)
        order[i] = i;
    for (auto i = 1u; i < N; i++)
        for (auto j = i; j > 0 && is_less(names[order[j]], names[order[j - 1]]); j--)
            std::swap(order[j], order[j - 1]);
    return order;
}

template<template<typename...> class Receiver, typename Indices, typename... Ts> struct sort_by_name;

template<template<typename...> class Receiver, usize... Is, typename... Ts>
struct sort_by_name<Receiver, std::index_sequence<Is...>, Ts...>
{
    static_assert((floormat::is_name_portable<Ts> && ...), "type names differ between compilers");
    static constexpr std::array<u32, sizeof...(Ts)> order = sorted_indices<Ts...>();
    using type = Receiver<std::tuple_element_t<order[Is], std::tuple<Ts...>>...>;
};

} // namespace floormat::detail::sort_by_name_impl

namespace floormat {

template<template<typename...> class Receiver, typename... Ts>
using sort_by_name =
    detail::sort_by_name_impl::sort_by_name<Receiver, std::make_index_sequence<sizeof...(Ts)>, Ts...>::type;

} // namespace floormat
