#pragma once
#include "compat/assert.hpp"
#include <type_traits>
#include <bit>
#include <concepts>
#include <tuple>

namespace floormat::detail_Pack_output {

template<std::unsigned_integral T, size_t CAPACITY, size_t LEFT>
struct output
{
    static_assert(std::is_fundamental_v<T>);
    static_assert(CAPACITY > 0);
    static_assert(CAPACITY <= sizeof(T)*8);
    static_assert(LEFT <= CAPACITY);
    static constexpr size_t Capacity = CAPACITY, Left = LEFT;
    T value{0};
};

template<std::unsigned_integral T, size_t LENGTH>
struct output_field
{
    static_assert(LENGTH > 0);
    static constexpr size_t Length = LENGTH;
    T value;
};

template <size_t... Is>
constexpr std::index_sequence<sizeof...(Is)-1u-Is...> reverse_index_sequence(std::index_sequence<Is...>);
template <size_t N>
using make_reverse_index_sequence = decltype(reverse_index_sequence(std::make_index_sequence<N>{}));

template<typename T> struct is_output_field : std::bool_constant<false> {};
template<std::unsigned_integral T, size_t N> struct is_output_field<output_field<T, N>> : std::bool_constant<true> { static_assert(N > 0); };

template<std::unsigned_integral T, size_t Capacity, size_t Left, size_t I, size_t... Is, typename Tuple>
constexpr CORRADE_ALWAYS_INLINE T write_(const Tuple& tuple, output<T, Capacity, Left> st, std::index_sequence<I, Is...>)
{
    static_assert(Capacity > 0);
    static_assert(Left > 0);
    static_assert(Capacity <= sizeof(T)*8);
    static_assert(Left <= Capacity);
    static_assert(is_output_field<std::decay_t<decltype(std::get<I>(tuple))>>{});
    constexpr size_t N = std::tuple_element_t<I, Tuple>::Length;
    static_assert(N <= Left);

    T x = std::get<I>(tuple).value;
    fm_assert((size_t)std::bit_width(x) <= N);
    T value = T(T(st.value << N) | x);
    return write_(tuple, output<T, Capacity, Left - N>{value}, std::index_sequence<Is...>{});
}

template<std::unsigned_integral T, size_t Capacity, size_t Left, typename Tuple>
constexpr CORRADE_ALWAYS_INLINE T write_(const Tuple&, output<T, Capacity, Left> st, std::index_sequence<>)
{
    return st.value;
}

template<std::unsigned_integral T, size_t... Sizes>
constexpr T write(const std::tuple<output_field<T, Sizes>...>& tuple)
{
    constexpr size_t nbits = sizeof(T)*8;
    return write_(tuple, output<T, nbits, nbits>{T{0}}, make_reverse_index_sequence<sizeof...(Sizes)>{});
}

constexpr uint8_t write(const std::tuple<>&) = delete;

} // namespace floormat::detail_Pack_output
