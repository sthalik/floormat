#pragma once
#include "compat/move.hpp"

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

// std::ranges::ref_view and owning_view, except that a const view iterates its range as const.
template<typename R>
class ref_view
{
    R* _r;

public:
    constexpr explicit ref_view(R& r) noexcept;

    constexpr begin_t<R> begin() noexcept;
    constexpr begin_t<const R> begin() const noexcept;
    constexpr end_t<R> end() noexcept;
    constexpr end_t<const R> end() const noexcept;
};

template<typename R>
class owning_view
{
    R _r;

public:
    constexpr explicit owning_view(R&& r) noexcept(std::is_nothrow_move_constructible_v<R>);

    constexpr begin_t<R> begin() noexcept;
    constexpr begin_t<const R> begin() const noexcept;
    constexpr end_t<R> end() noexcept;
    constexpr end_t<const R> end() const noexcept;
};

template<typename R>
using all_t = std::conditional_t<std::is_lvalue_reference_v<R>,
                                 ref_view<std::remove_reference_t<R>>,
                                 owning_view<std::remove_reference_t<R>>>;

template<typename R>
constexpr ref_view<R>::ref_view(R& r) noexcept : _r{&r} {}

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
constexpr owning_view<R>::owning_view(R&& r) noexcept(std::is_nothrow_move_constructible_v<R>) : _r{move(r)} {}

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

} // namespace floormat::detail_range
