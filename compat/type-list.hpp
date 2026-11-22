#pragma once
#include "is-one-of.hpp"
#include <type_traits>

namespace floormat {

template<typename... Ts> struct type_list {};

} // namespace floormat

namespace floormat::detail::type_list_impl {

template<typename T, typename... Ts>
consteval usize index_of()
{
    static_assert(is_one_of<T, Ts...>, "type isn't in the list");
    static_assert(((usize)std::is_same_v<T, Ts> + ... + 0uz) <= 1, "type appears more than once in the list");
    constexpr bool same[] = { std::is_same_v<T, Ts>..., false };
    usize i = 0;
    while (i < sizeof...(Ts) && !same[i])
        i++;
    return i;
}

template<template<typename...> class Receiver, typename Unique, typename... Ts> struct remove_duplicates;

template<template<typename...> class Receiver, typename... Us>
struct remove_duplicates<Receiver, type_list<Us...>>
{
    using type = Receiver<Us...>;
};

template<template<typename...> class Receiver, typename... Us, typename T, typename... Ts>
struct remove_duplicates<Receiver, type_list<Us...>, T, Ts...>:
    std::conditional_t<is_one_of<T, Us...>,
                       remove_duplicates<Receiver, type_list<Us...>, Ts...>,
                       remove_duplicates<Receiver, type_list<Us..., T>, Ts...>>
{};

} // namespace floormat::detail::type_list_impl

namespace floormat {

template<typename T, typename... Ts>
constexpr inline usize index_of = detail::type_list_impl::index_of<T, Ts...>();

template<typename A, typename B>
extern const bool is_subset_of;

template<typename... As, typename... Bs>
constexpr inline bool is_subset_of<type_list<As...>, type_list<Bs...>> = (is_one_of<As, Bs...> && ...);

template<template<typename...> class Receiver, typename... Ts>
using remove_duplicates = detail::type_list_impl::remove_duplicates<Receiver, type_list<>, Ts...>::type;

} // namespace floormat
