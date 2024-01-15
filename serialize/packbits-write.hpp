#pragma once
#include <concepts>

namespace floormat::detail_Pack_output {

template<std::unsigned_integral T>
struct output
{
    T value = 0;
    uint8_t capacity = sizeof(T)*8;

    constexpr inline output next(T x, uint8_t bits) const
    {
        fm_assert(bits > 0 && bits <= capacity);
        auto val = value;
        val <<= bits;
        T x_ = T(x & (T{1} << bits)- T{1});
        fm_assert(x_ == x);
        val |= x_;
        return { val | x_, capacity - bits };
    }
};





} // namespace floormat::detail_Pack_output
