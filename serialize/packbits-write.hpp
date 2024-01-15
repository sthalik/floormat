#pragma once
#include "packbits.hpp"

namespace floormat::detail_Pack {

template<std::unsigned_integral T, size_t CAPACITY>
struct output
{
    static_assert(CAPACITY <= sizeof(T)*8);
    static constexpr size_t Capacity = CAPACITY;
    T value;

    template<size_t N>
    constexpr void set(T x) const
    {
        static_assert(N > 0);
        static_assert(N <= sizeof(T)*8);
        static_assert(N <= Capacity);
        if constexpr(CAPACITY < sizeof(T)*8)
            value <<= CAPACITY;
        T x_ = T(x & (1 << N)-1);
        fm_assert(x_ == x);
        value |= x;
    }
    template<size_t N> using next = output<T, CAPACITY - N>;
};

} // namespace floormat::detail_Pack
