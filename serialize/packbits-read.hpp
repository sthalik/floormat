#pragma once
#include "packbits-impl.hpp"
#include "compat/assert.hpp"
#include <type_traits>
#include <concepts>
#include <tuple>
#include <Corrade/Utility/Move.h>

namespace floormat::Pack_impl {

template<std::unsigned_integral T, size_t LENGTH>
struct input_field final
{
    static_assert(LENGTH > 0);
    static_assert(LENGTH <= sizeof(T)*8);
    static constexpr size_t Length = LENGTH;
    T value;
    constexpr T operator*() const { return value; }
    constexpr operator T() const { return value; }
};

template<std::unsigned_integral T, size_t LEFT>
struct input
{
    static_assert(LEFT > 0);
    static_assert(LEFT <= sizeof(T)*8);
    static constexpr size_t Left = LEFT;

    T value;

    template<size_t N>
    struct next_
    {
        static_assert(N <= LEFT);
        using type = input<T, LEFT - N>;
    };
    template<size_t N> using next = typename next_<N>::type;

    template<size_t N>
    constexpr CORRADE_ALWAYS_INLINE T get() const
    {
        static_assert(N > 0);
        static_assert(N <= sizeof(T)*8);
        static_assert(N <= LEFT);
        return T(value & (T{1} << N) - T{1});
    }

    template<size_t N>
    constexpr T advance() const
    {
        static_assert(N <= sizeof(T)*8);
        static_assert(N <= LEFT);
        return T(value >> N);
    }

    constexpr bool operator==(const input&) const noexcept = default;
    [[nodiscard]] constexpr inline bool check_zero() const { return value == T{0}; }
};

template<std::unsigned_integral T>
struct input<T, 0>
{
    static constexpr size_t Left = 0;
    using Type = T;
    T value;

    template<size_t N> [[maybe_unused]] constexpr T get() const = delete;
    template<size_t N> [[maybe_unused]] constexpr T advance() const = delete;
    constexpr bool operator==(const input&) const noexcept = default;
    [[nodiscard]] constexpr inline bool check_zero() const { return true; }

    template<size_t N> struct next
    {
        static_assert(N == (size_t)-1, "reading past the end");
        static_assert(N != (size_t)-1);
    };
};

template<typename T> struct is_input_field : std::bool_constant<false> {};
template<std::unsigned_integral T, size_t N> struct is_input_field<input_field<T, N>> : std::bool_constant<true> { static_assert(N > 0); };

template<std::unsigned_integral T, typename Tuple, size_t Left, size_t I, size_t... Is>
constexpr CORRADE_ALWAYS_INLINE void read_(Tuple&& tuple, input<T, Left> st, std::index_sequence<I, Is...>)
{
    using U = std::decay_t<Tuple>;
    static_assert(Left <= sizeof(T)*8, "bits to read count too large");
    static_assert(Left > 0, "too many bits to write");
    static_assert(std::tuple_size_v<U> >= sizeof...(Is)+1, "index count larger than tuple element count");
    static_assert(I < std::tuple_size_v<U>, "too few tuple elements");
    using Field = std::decay_t<std::tuple_element_t<I, U>>;
    static_assert(is_input_field<Field>::value, "tuple element must be input<T, N>");
    constexpr size_t Size = Field::Length;
    static_assert(Size <= Left, "data type too small");
    using next_type = typename input<T, Left>::template next<Size>;
    std::get<I>(tuple).value = st.template get<Size>();
    T next_value = st.template advance<Size>();
    read_(Utility::forward<Tuple>(tuple), next_type{ next_value }, std::index_sequence<Is...>{});
}

template<std::unsigned_integral T, typename Tuple, size_t Left>
constexpr CORRADE_ALWAYS_INLINE void read_(Tuple&&, input<T, Left> st, std::index_sequence<>)
{
    if (!st.check_zero()) [[unlikely]]
        throw_on_read_nonzero();
}

} // namespace floormat::Pack_impl

namespace floormat {

template<std::unsigned_integral T, typename Tuple>
constexpr void pack_read(Tuple&& tuple, T value)
{
    constexpr size_t nbits = sizeof(T)*8,
                     tuple_size = std::tuple_size_v<std::decay_t<Tuple>>;
    Pack_impl::read_(Utility::forward<Tuple>(tuple),
                     Pack_impl::input<T, nbits>{value},
                     std::make_index_sequence<tuple_size>{});
}

} // namespace floormat
