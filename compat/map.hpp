#pragma once
#include "array-size.hpp"
#include "unroll.hpp"
#include "invoke.hpp"
#include <concepts>
#include <tuple>
#include <utility>
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail::map {

template<typename T>
concept Tuple = requires {
    std::tuple_size<T>::value;
    usize{ std::tuple_size_v<T> };
    []<usize... I>(std::index_sequence<I...>)
        requires (requires { typename std::tuple_element_t<I, T>; } && ...)
    {}(std::make_index_sequence<std::tuple_size_v<T>>{});
};

template<typename T> constexpr inline bool is_std_array = false;
template<typename T, usize N> constexpr inline bool is_std_array<std::array<T, N>> = true;

// MSVC keeps the stores that zero transform's default-constructed elements, though the loop overwrites each one.
#if defined _MSC_VER && !defined __clang__
constexpr inline bool transform_needs_trivial_ctor = true;
#else
constexpr inline bool transform_needs_trivial_ctor = false;
#endif

template<usize I, typename T>
CORRADE_ALWAYS_INLINE
constexpr decltype(auto) get_tuple_element(const T& tuple)
{
    if constexpr (requires { tuple.template get<I>(); })
        return tuple.template get<I>();
    else
        return get<I>(tuple);
}

template<typename Out, typename F, typename A, usize... Indexes>
CORRADE_ALWAYS_INLINE
constexpr auto map0(const F& fun, const A& array, std::index_sequence<Indexes...>)
{
    using out_element = std::remove_cvref_t<decltype( array_traits<Out>::get(std::declval<const Out&>(), 0) )>;
    return array_traits<Out>::make(static_cast<out_element>(invoke(fun, array_traits<A>::get(array, Indexes)))...);
}

template<typename Out, typename F, typename A>
[[deprecated("zero-length array!")]]
CORRADE_ALWAYS_INLINE
constexpr auto map0(const F&, const A&, std::index_sequence<>)
{
    return Out{};
}

// A loop leaves unrolling to the compiler. map0's pack expansion calls fun once per element in straight-line
// code, so a fun that makes a call puts N call instructions in the caller.
template<typename Out, typename F, typename A>
CORRADE_ALWAYS_INLINE
constexpr Out transform(const F& fun, const A& array)
{
    Out ret;
    for (usize i = 0; i < array_traits<A>::size; i++)
        ret[i] = static_cast<Out::value_type>(invoke(fun, array_traits<A>::get(array, i)));
    return ret;
}

} // namespace floormat::detail::map

namespace floormat {

template<typename Out = void, typename F, detail::map::Tuple T>
requires (!requires { detail::array_size_<T>::value; })
CORRADE_ALWAYS_INLINE
constexpr auto map(const F& fun, const T& tuple)
{
    return [&]<usize... I>(std::index_sequence<I...>) {
        using out = std::conditional_t<std::is_void_v<Out>,
            std::tuple<std::decay_t<decltype(invoke(fun, detail::map::get_tuple_element<I>(tuple)))>...>, Out>;
        static_assert(std::tuple_size_v<out> == sizeof...(I), "output container has the wrong size");
        return out{invoke(fun, detail::map::get_tuple_element<I>(tuple))...};
    }(std::make_index_sequence<std::tuple_size_v<T>>{});
}

template<typename Out = void, typename F, typename A>
requires requires (const F& fun, const A& array) {
    detail::array_size_<std::remove_cvref_t<A>>::value;
    invoke(fun, array_traits<std::remove_cvref_t<A>>::get(array, 0));
}
CORRADE_ALWAYS_INLINE
constexpr auto map(const F& fun, const A& array)
{
    using traits = array_traits<std::remove_cvref_t<A>>;
    using result = std::decay_t<decltype( invoke(fun, traits::get(array, 0)) )>;
    using out = std::conditional_t<std::is_void_v<Out>, typename traits::template rebind<result>, Out>;
    static_assert(!std::is_same_v<result, void>);
    static_assert(sizeof(result));
    static_assert(static_array_size<out> == traits::size, "output container has the wrong size");
    if constexpr (detail::map::is_std_array<out> && traits::size > 0 && std::is_default_constructible_v<out> &&
                  std::is_move_assignable_v<out> &&
                  (!detail::map::transform_needs_trivial_ctor || std::is_trivially_default_constructible_v<out>))
        return detail::map::transform<out>(fun, array);
    else
        return detail::map::map0<out>(fun, array, std::make_index_sequence<traits::size>{});
}

} // namespace floormat
