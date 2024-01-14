#pragma once
#include <type_traits>
#include <concepts>
#include <tuple>
#include "compat/assert.hpp"

namespace floormat::Pack {
template<std::unsigned_integral T, size_t N> struct Bits_;
} // namespace floormat::Pack

namespace floormat::detail_Pack {

using namespace floormat::Pack;


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
    [[nodiscard]] constexpr inline bool check_zero() const { return value == T(0); }

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
    [[nodiscard]] constexpr inline bool check_zero() const { return true; }

    template<size_t N> struct next
    {
        static_assert(N == 0, "reading past the end");
        static_assert(N != 0, "reading past the end");
    };
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

template<typename T, typename Place, size_t Left, size_t I, size_t... Is, size_t Size, typename... Sizes>
constexpr void assign_tuple2(Place& p, Storage<T, Left> st, std::index_sequence<I, Is...>, Bits_<T, Size>, Sizes... sizes)
{
    using S = Storage<T, Left>;
    using next_type = typename S::template next<Size>;
    get<I>(p) = st.template get<Size>();
    T next_value = st.template advance<Size>();
    assign_tuple2(p, next_type{next_value}, std::index_sequence<Is...>{}, sizes...);
}

template<typename T, typename Place, size_t Left>
constexpr void assign_tuple2(Place&, Storage<T, Left>, std::index_sequence<>)
{
}

#if 0
template<typename T, typename Indexes, typename... Sizes> struct assign_tuple;

template<typename T, size_t Index, size_t... Indexes, size_t Size, typename... Sizes>
struct assign_tuple<T, std::index_sequence<Index, Indexes...>, Bits_<T, Size>, Sizes...>
{
    static_assert(Size <= sizeof(T)*8, "bit count can't be larger than sizeof(T)*8");
    static_assert(Size > 0, "bit count can't be zero");

    template<typename Place, size_t Left>
    static constexpr inline void do_tuple(Place& p, Storage<T, Left> st)
    {
        static_assert(requires (Place& p) { std::get<0>(p) = T{0}; });
        static_assert(std::tuple_size_v<Place> >= sizeof...(Indexes) + 1);
        static_assert(Size <= Left, "not enough bits for element");

        get<Index>(p) = st.template get<Size>(st.value);
        using Next = typename Storage<T, Left>::template next<Size>;
        assign_tuple<T, std::index_sequence<Indexes...>, Sizes...>::
            template do_tuple<Place, Next>(p, Next{st.template advance<Size>()});
    }

    static constexpr bool is_empty = false;
};

template<typename T, size_t Index, size_t... Indexes>
struct assign_tuple<T, std::index_sequence<Index, Indexes...>>
{
    static_assert(sizeof(T) == (size_t)-1, "too few lhs elements");
    static_assert(sizeof(T) != (size_t)-1);
};

template<typename T, size_t Size, typename... Sizes>
struct assign_tuple<T, std::index_sequence<>, Bits_<T, Size>, Sizes...>
{
    static_assert(sizeof(T) == (size_t)-1, "too few rhs elements");
    static_assert(sizeof(T) != (size_t)-1);
};

template<typename T>
struct assign_tuple<T, std::index_sequence<>>
{
    template<typename Place, size_t Left>
    static constexpr inline void do_tuple(Place&, Storage<T, Left> st)
    {
        fm_assert(st.check_zero());
    }

    static constexpr bool is_empty = true;
    using type = T;
};

#endif

} // namespace floormat::detail_Pack

namespace floormat::Pack {

template<std::unsigned_integral T, size_t N>
struct Bits_ final
{
    static_assert(std::is_fundamental_v<T>);
    static_assert(N > 0);
    static_assert(N < sizeof(T)*8);

    using type = T;
    static constexpr auto bits = N;
};

} // namespace floormat::Pack
