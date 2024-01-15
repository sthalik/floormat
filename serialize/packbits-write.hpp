#pragma once
#include "compat/assert.hpp"
#include <type_traits>
#include <tuple>
#include <concepts>

namespace floormat::detail_Pack_output {

template<size_t N>
struct output_bits final
{
    static_assert(N > 0);
    static_assert(N < sizeof(size_t)*8);

    static constexpr size_t length = N;
};

template<std::unsigned_integral T, size_t CAPACITY>
struct output
{
    static_assert(std::is_fundamental_v<T>);
    static_assert(CAPACITY <= sizeof(T)*8);
    static constexpr size_t Capacity = CAPACITY;

    T value{0};

    template<size_t N>
    struct next_
    {
        static_assert(N <= sizeof(T)*8);
        static_assert(N > 0);
        static_assert(N <= CAPACITY);
        using type = output<T, CAPACITY - N>;
    };
    template<size_t N> using next = typename next_<N>::type;

    template<size_t N>
    constexpr T set(T x, output_bits<N>) const
    {
        static_assert(N <= CAPACITY, "data type too small");
        static_assert(N > 0);
        T value_{value};
        if constexpr(CAPACITY != sizeof(T)*8)
            value_ <<= N;
        auto x_ = T(x & (T{1}<<N)-T{1});
        fm_assert(x_ == x);
        value_ |= x_;
        return value_;
    }
};

template<typename T, size_t LENGTH>
struct output_field
{
    T value;
    static constexpr size_t Length = LENGTH;
};

template<std::unsigned_integral Type, typename Tuple> struct count_bits;


template<std::unsigned_integral Int, size_t N, typename... Ts> struct count_bits<Int, std::tuple<output_field<Int, N>, Ts...>>
{
    static constexpr size_t length = N + count_bits<Int, std::tuple<Ts...>>::length;
    static_assert(length <= sizeof(Int)*8);
};

template<std::unsigned_integral Int> struct count_bits<Int, std::tuple<>>
{
    static constexpr size_t length = 0;
};

template <std::size_t ... Is>
constexpr std::index_sequence<sizeof...(Is)-1uz-Is...> reverse_index_sequence(std::index_sequence<Is...> const&);

template <std::size_t N>
using make_reverse_index_sequence = decltype(reverse_index_sequence(std::make_index_sequence<N>{}));

template<typename T, size_t Left, size_t I, size_t... Is, typename Tuple>
constexpr T write_(output<T, Left> st, std::index_sequence<I, Is...>, const Tuple& tuple)
{
    constexpr size_t N = std::tuple_element_t<I, Tuple>::Length;
    if constexpr(Left != sizeof(T)*8)
        st.value <<= N;
    T x = std::get<I>(tuple).value;
    T value = st.set(x, output_bits<N>{});
    using next = typename output<T, Left>::template next<N>;
    return write_(next{value}, std::index_sequence<Is...>{}, tuple);
}

template<typename T, size_t Left, typename Tuple>
constexpr T write_(output<T, Left> st, std::index_sequence<>, const Tuple&)
{
    return st.value;
}

} // namespace floormat::detail_Pack_output
