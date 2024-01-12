#pragma once
#include "compat/assert.hpp"
#include <type_traits>
#include <concepts>

namespace floormat::Pack {
template<std::unsigned_integral T, uint8_t N> struct Bits_;
} // namespace floormat::Pack

namespace floormat::detail_Pack {

using namespace floormat::Pack;
template<std::unsigned_integral T, size_t Sum, typename... Xs> struct check_size_overflow;

template<std::unsigned_integral T, std::unsigned_integral U, size_t Sum, uint8_t N, typename... Xs>
struct check_size_overflow<T, Sum, Bits_<U, N>, Xs...>
{
    static_assert(std::is_same_v<T, U>);
    static constexpr auto acc = Sum + size_t{N};
    using next_check = check_size_overflow<T, acc, Xs...>;
    static constexpr auto size = next_check::size;
    static constexpr bool result = next_check::result;
};

template<std::unsigned_integral T, size_t Sum>
struct check_size_overflow<T, Sum>
{
    static constexpr size_t size = Sum;
    static constexpr bool result = Sum <= sizeof(T)*8;
};

template<std::unsigned_integral T, size_t Capacity>
struct Storage
{
    static_assert(Capacity <= sizeof(T)*8);
    T value;

    template<size_t N>
    constexpr T get()
    {
        static_assert(N <= sizeof(T)*8);
        static_assert(N <= Capacity);
        return T(value & (T(1) << N) - T(1));
    }

    template<size_t N>
    constexpr T advance()
    {
        static_assert(N <= sizeof(T)*8);
        static_assert(N <= Capacity);
        return T(value >> N);
    }

    template<size_t N> [[maybe_unused]] constexpr bool check_zero() = delete;

    template<size_t N>
    using next = Storage<T, Capacity - N>;
};

template<std::unsigned_integral T>
struct Storage<T, 0>
{
    T value;

    template<size_t N> [[maybe_unused]] constexpr T get() = delete;
    template<size_t N> [[maybe_unused]] constexpr T advance() = delete;

    template<size_t N> constexpr inline bool check_zero()
    {
        fm_assert(value == T(0));
        return true;
    }

    template<size_t N> struct next
    {
        static_assert(!std::is_same_v<T, void>, "reading past the end");
        static_assert( std::is_same_v<T, void>, "can't happen!");
    };
};

} // namespace floormat::detail_Pack

namespace floormat::Pack {

template<std::unsigned_integral T, uint8_t N>
struct Bits_ final
{
    static_assert(std::is_fundamental_v<T>);
    static_assert(N > 0);
    static_assert(N < sizeof(T)*8);

    using type = T;
    static constexpr auto bits = N;
};



} // namespace floormat::Pack
