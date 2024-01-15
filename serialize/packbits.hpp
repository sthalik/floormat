#pragma once
#include <concepts>
#include <utility>

namespace floormat::detail_Pack {

template<std::unsigned_integral T, size_t N>
struct bits final
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

} // namespace floormat::detail_Pack

namespace floormat::pack {



} // namespace floormat::pack
