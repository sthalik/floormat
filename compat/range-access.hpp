#pragma once
#include "move.hpp"
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail::range {

template<typename T> std::add_rvalue_reference_t<T> declval() noexcept;

// Ordinary lookup from adl_begin and adl_end stops here, so only ADL finds a range's own begin and end.
void begin() = delete;
void end() = delete;

template<typename R> concept member_begin = requires(R& r) { r.begin(); };
template<typename R> concept adl_begin = requires(R& r) { begin(r); };
template<typename R> concept member_end = requires(R& r) { r.end(); };
template<typename R> concept adl_end = requires(R& r) { end(r); };
template<typename R> concept member_size = requires(R& r) { r.size(); };

struct begin_fn
{
    template<typename R> requires std::is_array_v<R> || member_begin<R> || adl_begin<R>
    constexpr auto operator()(R& r) const noexcept;
};

struct end_fn
{
    template<typename R> requires std::is_array_v<R> || member_end<R> || adl_end<R>
    constexpr auto operator()(R& r) const noexcept;
};

struct size_fn
{
    template<typename R> requires std::is_array_v<R> || member_size<R>
    constexpr usize operator()(R& r) const noexcept;
};

template<typename R> requires std::is_array_v<R> || member_begin<R> || adl_begin<R>
constexpr auto begin_fn::operator()(R& r) const noexcept
{
    if constexpr (std::is_array_v<R>)
        return r + 0;
    else if constexpr (member_begin<R>)
        return r.begin();
    else
        return begin(r);
}

template<typename R> requires std::is_array_v<R> || member_end<R> || adl_end<R>
constexpr auto end_fn::operator()(R& r) const noexcept
{
    if constexpr (std::is_array_v<R>)
        return r + std::extent_v<R>;
    else if constexpr (member_end<R>)
        return r.end();
    else
        return end(r);
}

template<typename R> requires std::is_array_v<R> || member_size<R>
constexpr usize size_fn::operator()(R& r) const noexcept
{
    if constexpr (std::is_array_v<R>)
        return std::extent_v<R>;
    else
        return usize(r.size());
}

} // namespace floormat::detail::range

namespace floormat {

// Inline, so that a floormat class can still declare a hidden friend named begin, end or size.
inline namespace cpo {
constexpr inline detail::range::begin_fn begin{};
constexpr inline detail::range::end_fn end{};
constexpr inline detail::range::size_fn size{};
} // namespace cpo

template<typename R> using iterator_t = decltype(floormat::begin(detail::range::declval<R&>()));
template<typename R> using sentinel_t = decltype(floormat::end(detail::range::declval<R&>()));

template<typename R>
concept range = requires(R& r) { floormat::begin(r); floormat::end(r); };

template<typename R>
concept sized_range = range<R> && requires(R& r) { floormat::size(r); };

template<typename S, typename I>
concept sized_sentinel_for = requires(const S& s, const I& i) { static_cast<usize>(s - i); };

// Tests only the operations floormat's views call, so std::random_access_iterator may reject a type this accepts.
template<typename I>
concept random_access_iterator = sized_sentinel_for<I, I> && requires(I& it, const I& c, usize n) { *it; ++it; c[n]; c + n; };

template<typename R>
concept random_access_range = range<R> && random_access_iterator<iterator_t<R>>;

} // namespace floormat

namespace floormat::detail::range {

// std::ranges::ref_view and owning_view, except that a const view iterates its range as const.
template<typename R>
class ref_view
{
    R* _r;

public:
    constexpr explicit ref_view(R& r) noexcept;

    constexpr R& base() noexcept;
    constexpr const R& base() const noexcept;
    constexpr iterator_t<R> begin() noexcept;
    constexpr iterator_t<const R> begin() const noexcept;
    constexpr sentinel_t<R> end() noexcept;
    constexpr sentinel_t<const R> end() const noexcept;
    constexpr usize size() const noexcept requires sized_range<const R>;
};

template<typename R>
class owning_view
{
    R _r;

public:
    constexpr explicit owning_view(R&& r) noexcept(std::is_nothrow_move_constructible_v<R>);

    constexpr R& base() noexcept;
    constexpr const R& base() const noexcept;
    constexpr iterator_t<R> begin() noexcept;
    constexpr iterator_t<const R> begin() const noexcept;
    constexpr sentinel_t<R> end() noexcept;
    constexpr sentinel_t<const R> end() const noexcept;
    constexpr usize size() const noexcept requires sized_range<const R>;
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
constexpr iterator_t<R> ref_view<R>::begin() noexcept
{
    return floormat::begin(*_r);
}

template<typename R>
constexpr iterator_t<const R> ref_view<R>::begin() const noexcept
{
    return floormat::begin(static_cast<const R&>(*_r));
}

template<typename R>
constexpr sentinel_t<R> ref_view<R>::end() noexcept
{
    return floormat::end(*_r);
}

template<typename R>
constexpr sentinel_t<const R> ref_view<R>::end() const noexcept
{
    return floormat::end(static_cast<const R&>(*_r));
}

template<typename R>
constexpr usize ref_view<R>::size() const noexcept requires sized_range<const R>
{
    return floormat::size(static_cast<const R&>(*_r));
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
constexpr iterator_t<R> owning_view<R>::begin() noexcept
{
    return floormat::begin(_r);
}

template<typename R>
constexpr iterator_t<const R> owning_view<R>::begin() const noexcept
{
    return floormat::begin(_r);
}

template<typename R>
constexpr sentinel_t<R> owning_view<R>::end() noexcept
{
    return floormat::end(_r);
}

template<typename R>
constexpr sentinel_t<const R> owning_view<R>::end() const noexcept
{
    return floormat::end(_r);
}

template<typename R>
constexpr usize owning_view<R>::size() const noexcept requires sized_range<const R>
{
    return floormat::size(_r);
}

} // namespace floormat::detail::range
