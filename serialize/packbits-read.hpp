#pragma once
#include <type_traits>
#include <concepts>
#include <tuple>
#include <utility>
#include "compat/assert.hpp"

namespace floormat::detail_Pack_input {

template<std::unsigned_integral T, size_t N>
struct input_bits final
{
    static_assert(std::is_fundamental_v<T>);
    static_assert(N > 0);
    static_assert(N < sizeof(T)*8);

    using type = T;
};

template<std::unsigned_integral T, size_t N>
struct make_tuple_type_
{
    template<size_t> using index_to_type = T;
    template<typename> struct aux;
    template<size_t... Is> struct aux<std::index_sequence<Is...>>
    {
        static_assert(sizeof...(Is) > 0);
        using type = std::tuple<index_to_type<Is>...>;
    };
    using Seq = typename aux<std::make_index_sequence<N>>::type;
};
template<std::unsigned_integral T, size_t N> using make_tuple_type = typename make_tuple_type_<T, N>::Seq;

template<typename... Ts> struct empty_pack_tuple {};

template<std::unsigned_integral T, size_t CAPACITY>
struct input
{
    static_assert(CAPACITY <= sizeof(T)*8);
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

    constexpr bool operator==(const input&) const noexcept = default;
    [[nodiscard]] constexpr inline bool check_zero() const { return value == T(0); }

    template<size_t N>
    struct next_
    {
        static_assert(N <= Capacity);
        using type = input<T, Capacity - N>;
    };
    template<size_t N> using next = typename next_<N>::type;
};

template<std::unsigned_integral T>
struct input<T, 0>
{
    using Type = T;
    static constexpr size_t Capacity = 0;
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

template<std::unsigned_integral T, typename Place, size_t Left, size_t I, size_t... Is, size_t Size, typename... Sizes>
constexpr void read(Place& p, input<T, Left> st, std::index_sequence<I, Is...>, empty_pack_tuple<input_bits<T, Size>, Sizes...>)
{
    static_assert(sizeof...(Is) == sizeof...(Sizes));
    static_assert(Size <= Left, "data type too small");
    static_assert(I < std::tuple_size_v<Place>, "too few tuple members");
    using S = input<T, Left>;
    using next_type = typename S::template next<Size>;
    get<I>(p) = st.template get<Size>();
    T next_value = st.template advance<Size>();
    read(p, next_type{ next_value }, std::index_sequence<Is...>{}, empty_pack_tuple<Sizes...>{});
}

template<std::unsigned_integral T, typename Place, size_t Left>
constexpr void read(Place&, input<T, Left> st, std::index_sequence<>, empty_pack_tuple<>)
{
    fm_assert(st.check_zero());
}

template<std::unsigned_integral T, typename Place, size_t Left, size_t... Is, typename... Sizes>
requires(sizeof...(Is) != sizeof...(Sizes))
constexpr void read(Place&, input<T, Left>, std::index_sequence<Is...>, empty_pack_tuple<Sizes...>) = delete;

template<std::unsigned_integral T, size_t... Ns> using make_pack = empty_pack_tuple<input_bits<T, Ns>...>;

} // namespace floormat::detail_Pack_input

namespace floormat::pack {



} // namespace floormat::pack
