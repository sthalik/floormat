#pragma once
#include "is-complete.hpp"
#include <utility>
#include <type_traits>
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat {

struct IgnoreT
{
    struct Init {};
    constexpr explicit IgnoreT(Init) noexcept;
    template<typename T> constexpr const IgnoreT& operator=(T&&) const noexcept;
};

constexpr IgnoreT::IgnoreT(Init) noexcept {}

template<typename T>
constexpr const IgnoreT& IgnoreT::operator=(T&&) const noexcept
{
    return *this;
}

constexpr inline IgnoreT Ignore{IgnoreT::Init{}};

} // namespace floormat

namespace floormat::detail_tie {

template<typename S>
consteval usize tuple_size_of()
{
    static_assert(IsComplete<std::tuple_size<S>>::value,
                  "tie(): std::tuple_size isn't defined for the source type; "
                  "for Corrade containers, include <cr/StructuredBindings.h>");
    if constexpr(IsComplete<std::tuple_size<S>>::value)
        return std::tuple_size_v<S>;
    else
        return (usize)-1;
}

template<usize I, typename T> struct tie_leaf { T& value; };

template<typename Seq, typename... Ts> struct tie_impl;

template<usize... Is, typename... Ts>
struct tie_impl<std::index_sequence<Is...>, Ts...> : tie_leaf<Is, Ts>...
{
    template<typename Src> constexpr void assign(Src&& src) const;
};

template<usize... Is, typename... Ts>
template<typename Src>
constexpr void tie_impl<std::index_sequence<Is...>, Ts...>::assign(Src&& src) const
{
    ((static_cast<const tie_leaf<Is, Ts>&>(*this).value = get<Is>(static_cast<Src&&>(src))), ...);
}

template<typename... Ts>
struct tie_proxy : tie_impl<std::index_sequence_for<Ts...>, Ts...>
{
    template<typename Src>
    requires (detail_tie::tuple_size_of<std::remove_cvref_t<Src>>() == sizeof...(Ts))
    constexpr void operator=(Src&& src) const;
};

template<typename... Ts>
template<typename Src>
requires (detail_tie::tuple_size_of<std::remove_cvref_t<Src>>() == sizeof...(Ts))
constexpr void tie_proxy<Ts...>::operator=(Src&& src) const
{
    this->assign(static_cast<Src&&>(src));
}

} // namespace floormat::detail_tie

namespace floormat {

template<typename... Ts>
constexpr detail_tie::tie_proxy<Ts...> tie(Ts&... refs) noexcept
{
    return {{ {refs}... }};
}

} // namespace floormat
