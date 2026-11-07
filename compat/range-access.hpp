#pragma once
#include "move.hpp"
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail_range {

template<typename T> std::add_rvalue_reference_t<T> declval() noexcept;

template<typename R>
constexpr auto begin_of(R& r) noexcept
{
    if constexpr (std::is_array_v<R>)
        return r + 0;
    else if constexpr (requires { r.begin(); })
        return r.begin();
    else
        return begin(r);
}

template<typename R>
constexpr auto end_of(R& r) noexcept
{
    if constexpr (std::is_array_v<R>)
        return r + std::extent_v<R>;
    else if constexpr (requires { r.end(); })
        return r.end();
    else
        return end(r);
}

template<typename R> using begin_t = decltype(begin_of(declval<R&>()));
template<typename R> using end_t = decltype(end_of(declval<R&>()));

template<typename R>
concept sized = std::is_array_v<R> || requires(R& r) { r.size(); };

template<typename R>
constexpr usize size_of(R& r) noexcept requires sized<R>
{
    if constexpr (std::is_array_v<R>)
        return std::extent_v<R>;
    else
        return usize(r.size());
}

// std::ranges::ref_view and owning_view, except that a const view iterates its range as const.
template<typename R>
class ref_view
{
    R* _r;

public:
    constexpr explicit ref_view(R& r) noexcept;

    constexpr R& base() noexcept;
    constexpr const R& base() const noexcept;
    constexpr begin_t<R> begin() noexcept;
    constexpr begin_t<const R> begin() const noexcept;
    constexpr end_t<R> end() noexcept;
    constexpr end_t<const R> end() const noexcept;
    constexpr usize size() const noexcept requires sized<const R>;
};

template<typename R>
class owning_view
{
    R _r;

public:
    constexpr explicit owning_view(R&& r) noexcept(std::is_nothrow_move_constructible_v<R>);

    constexpr R& base() noexcept;
    constexpr const R& base() const noexcept;
    constexpr begin_t<R> begin() noexcept;
    constexpr begin_t<const R> begin() const noexcept;
    constexpr end_t<R> end() noexcept;
    constexpr end_t<const R> end() const noexcept;
    constexpr usize size() const noexcept requires sized<const R>;
};

template<typename R>
using all_t = std::conditional_t<std::is_lvalue_reference_v<R>,
                                 ref_view<std::remove_reference_t<R>>,
                                 owning_view<std::remove_reference_t<R>>>;

// The element count that a range's type fixes, or 0 if it fixes none.
template<typename R> constexpr usize static_extent = 0;
template<typename T, usize N> constexpr usize static_extent<T[N]> = N;
template<typename R> requires requires { std::tuple_size<R>::value; }
constexpr usize static_extent<R> = std::tuple_size<R>::value;
template<typename R> constexpr usize static_extent<ref_view<R>> = static_extent<std::remove_cv_t<R>>;
template<typename R> constexpr usize static_extent<owning_view<R>> = static_extent<std::remove_cv_t<R>>;

template<typename R>
constexpr ref_view<R>::ref_view(R& r) noexcept : _r{&r} {}

template<typename R>
constexpr R& ref_view<R>::base() noexcept
{
    return *_r;
}

template<typename R>
constexpr const R& ref_view<R>::base() const noexcept
{
    return *_r;
}

template<typename R>
constexpr begin_t<R> ref_view<R>::begin() noexcept
{
    return begin_of(*_r);
}

template<typename R>
constexpr begin_t<const R> ref_view<R>::begin() const noexcept
{
    return begin_of(static_cast<const R&>(*_r));
}

template<typename R>
constexpr end_t<R> ref_view<R>::end() noexcept
{
    return end_of(*_r);
}

template<typename R>
constexpr end_t<const R> ref_view<R>::end() const noexcept
{
    return end_of(static_cast<const R&>(*_r));
}

template<typename R>
constexpr usize ref_view<R>::size() const noexcept requires sized<const R>
{
    return size_of(static_cast<const R&>(*_r));
}

template<typename R>
constexpr owning_view<R>::owning_view(R&& r) noexcept(std::is_nothrow_move_constructible_v<R>) : _r{floormat::move(r)} {}

template<typename R>
constexpr R& owning_view<R>::base() noexcept
{
    return _r;
}

template<typename R>
constexpr const R& owning_view<R>::base() const noexcept
{
    return _r;
}

template<typename R>
constexpr begin_t<R> owning_view<R>::begin() noexcept
{
    return begin_of(_r);
}

template<typename R>
constexpr begin_t<const R> owning_view<R>::begin() const noexcept
{
    return begin_of(_r);
}

template<typename R>
constexpr end_t<R> owning_view<R>::end() noexcept
{
    return end_of(_r);
}

template<typename R>
constexpr end_t<const R> owning_view<R>::end() const noexcept
{
    return end_of(_r);
}

template<typename R>
constexpr usize owning_view<R>::size() const noexcept requires sized<const R>
{
    return size_of(_r);
}

} // namespace floormat::detail_range
