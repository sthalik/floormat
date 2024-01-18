#pragma once
#include "packbits-impl.hpp"
#include "compat/reverse-index-sequence.hpp"
#include <type_traits>
#include <bit>
#include <concepts>
#include <tuple>

namespace floormat::Pack_impl {

template<std::unsigned_integral T, size_t CAPACITY, size_t LEFT>
struct output
{
    static_assert(LEFT >= 0);
    static_assert(LEFT <= CAPACITY);
    static_assert(CAPACITY <= sizeof(T)*8);
    static constexpr size_t Capacity = CAPACITY, Left = LEFT;
    T value{0};
};

template<std::unsigned_integral T, size_t LENGTH>
struct output_field
{
    static_assert(LENGTH > 0);
    static_assert(LENGTH <= sizeof(T)*8);
    static constexpr size_t Length = LENGTH;
    T value;
};

template<typename T> struct is_output_field : std::bool_constant<false> {};
template<std::unsigned_integral T, size_t N> struct is_output_field<output_field<T, N>> : std::bool_constant<true> { static_assert(N > 0); };

template<typename Field>
requires requires (const Field& x) {
    { size_t{Field::Length} > 0 };
    sizeof(std::decay_t<decltype(x.value)>);
    std::unsigned_integral<std::decay_t<decltype(x.value)>>;
}
struct is_output_field<Field> : std::bool_constant<true> {};

template<std::unsigned_integral T, size_t Capacity, size_t Left, size_t I, size_t... Is, typename Tuple>
constexpr CORRADE_ALWAYS_INLINE T write_(const Tuple& tuple, output<T, Capacity, Left> st, std::index_sequence<I, Is...>)
{
    static_assert(Capacity > 0);
    static_assert(Capacity <= sizeof(T)*8);
    static_assert(Left > 0, "too many bits to write");
    static_assert(Left <= Capacity, "too many bits to write");
    static_assert(I < std::tuple_size_v<Tuple>, "too few tuple elements");
    static_assert(is_output_field<std::decay_t<std::tuple_element_t<0, Tuple>>>{},
                  "tuple element must be output_field<T,N>");
    constexpr size_t N = std::tuple_element_t<I, Tuple>::Length;
    static_assert(N <= Left, "too many bits to write");

    T x = std::get<I>(tuple).value;

    if (!((size_t)std::bit_width(x) <= N)) [[unlikely]]
        throw_on_write_input_bit_overflow();
    T value = T(T(st.value << N) | x);
    return write_(tuple, output<T, Capacity, Left - N>{value}, std::index_sequence<Is...>{});
}

template<std::unsigned_integral T, size_t Capacity, size_t Left, typename Tuple>
constexpr CORRADE_ALWAYS_INLINE T write_(const Tuple&, output<T, Capacity, Left> st, std::index_sequence<>)
{
    return st.value;
}

} // namespace floormat::Pack_impl

namespace floormat {

template<typename Tuple>
requires requires (const Tuple& tuple) {
    std::tuple_size_v<Tuple> > size_t{0};
    Pack_impl::is_output_field<std::decay_t<decltype(std::get<0>(tuple))>>::value;
}
[[nodiscard]] constexpr auto pack_write(const Tuple& tuple)
{
    using Field = std::decay_t<std::tuple_element_t<0, Tuple>>;
    static_assert(Pack_impl::is_output_field<Field>{});
    using T = std::decay_t<decltype(std::declval<Field>().value)>;
    constexpr size_t nbits = sizeof(T)*8, tuple_size = std::tuple_size_v<Tuple>;
    return Pack_impl::write_(tuple,
                             Pack_impl::output<T, nbits, nbits>{T{0}},
                             make_reverse_index_sequence<tuple_size>{});
}

constexpr uint8_t pack_write(const std::tuple<>&) = delete;

} // namespace floormat
