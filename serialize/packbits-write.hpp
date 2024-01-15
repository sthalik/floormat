#pragma once
#include "compat/assert.hpp"
#include <type_traits>
#include <utility>
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
    static_assert(length <= sizeof(T)*8);
};

template<std::unsigned_integral Int> struct count_bits<Int, std::tuple<>>
{
    static constexpr size_t length = 0;
};

#if 0
template<typename T, size_t Left, typename Fields, size_t... Is>
constexpr T write_(output<T, Left> st, std::tuple<Fields..., output_field<T, F>> fields)
{
    T value0 = fields.get< std::tuple_size_v<std::tuple<Fields..., output_field<T, F>>>-1 >(fields).value;
    T value = st.set(value0, output_bits<F>{});
    using next = typename output<T, Left>::template next<F>;
    return write_(next{value}, );
}

template<typename T, size_t Left>
constexpr T write_(output<T, Left> st)
{
    return st.value;
}
#endif

} // namespace floormat::detail_Pack_output
