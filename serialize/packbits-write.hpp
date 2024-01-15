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
    constexpr T set(T x, output_bits<N>) const
    {
        static_assert(N <= CAPACITY, "data type too small");
        static_assert(N > 0);
        T value_{value};
        if constexpr(CAPACITY != sizeof(T)*8)
            value_ <<= CAPACITY;
        auto x_ = T(x & (T{1}<<N)-T{1});
        fm_assert(x_ == x);
        value_ |= x_;
        return value_;
    }

    template<size_t N>
    struct next_
    {
        static_assert(N > 0);
        static_assert(N <= CAPACITY);
        using type = output<T, CAPACITY - N>;
    };

    template<size_t N> using next = typename next_<N>::type;
};

template<typename T, size_t N>
using output_field = std::pair<T, output_bits<N>>;

template<typename... Ts> struct empty_pack_tuple {}; // todo copypasta

template<typename T, size_t Left, size_t F, typename... Fields>
constexpr T write_(output<T, Left> st, output_field<T, F> field, Fields... fields)
{
    T value = st.set(field.first, field.second);
    using next = typename output<T, Left>::template next<F>;
    return write_(next{value}, fields...);
}

template<typename T, size_t Left>
constexpr T write_(output<T, Left> st)
{
    return st.value;
}

} // namespace floormat::detail_Pack_output
