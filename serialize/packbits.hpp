#pragma once
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

template<std::unsigned_integral T, size_t CAPACITY>
struct Storage
{
    static_assert(CAPACITY <= sizeof(T)*8);

    using Type = T;
    static constexpr size_t Capacity = CAPACITY;
    T value;

    template<size_t N>
    constexpr T get() const
    {
        static_assert(N > 0);
        static_assert(N <= sizeof(T)*8);
        static_assert(N <= Capacity);
        return T(value & (T(1) << N) - T(1));
    }

    template<size_t N>
    constexpr T advance() const
    {
        static_assert(N <= sizeof(T)*8);
        static_assert(N <= Capacity);
        return T(value >> N);
    }

    constexpr bool operator==(const Storage&) const noexcept = default;
    constexpr bool check_zero() const = delete;

    template<size_t N> using next = Storage<T, Capacity - N>;
};

template<std::unsigned_integral T>
struct Storage<T, 0>
{
    using Type = T;
    static constexpr size_t Capacity = 0;
    T value;

    template<size_t N> [[maybe_unused]] constexpr T get() const = delete;
    template<size_t N> [[maybe_unused]] constexpr T advance() const = delete;
    constexpr bool operator==(const Storage&) const noexcept = default;

    [[nodiscard]] constexpr inline bool check_zero() const
    {
        return value == T(0);
    }

    template<size_t N> struct next
    {
        static_assert(N == 0, "reading past the end");
        static_assert(N != 0, "reading past the end");
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