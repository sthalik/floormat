#pragma once
#include "combinations.hpp"

namespace floormat::detail::pairwise {

template<typename V> class iterator;
template<typename R> class view;

struct sentinel {};

// operator* returns a reference into the iterator, as detail::combinations::iterator does.
template<typename V>
class iterator
{
    template<typename> friend class view;
    using element = detail::combinations::element<V>;

    element _e;
    sentinel_t<V> _end;

    constexpr explicit iterator(V& v) noexcept;

public:
    constexpr element& operator*() noexcept;
    constexpr const element& operator*() const noexcept;
    constexpr iterator& operator++() noexcept;
    constexpr bool operator==(sentinel) const noexcept;
};

template<typename R>
class view
{
    using base_type = detail::range::all_t<R>;

    base_type _base;

public:
    constexpr explicit view(R&& r) noexcept(std::is_nothrow_constructible_v<base_type, R>);

    constexpr iterator<base_type> begin() noexcept;
    constexpr iterator<const base_type> begin() const noexcept;
    constexpr sentinel end() const noexcept;
};

template<typename V>
constexpr iterator<V>::iterator(V& v) noexcept :
    _e{floormat::begin(v), floormat::begin(v)}, _end{floormat::end(v)}
{
    if (_e._second != _end)
        ++_e._second;
}

template<typename V>
constexpr auto iterator<V>::operator*() noexcept -> element&
{
    return _e;
}

template<typename V>
constexpr auto iterator<V>::operator*() const noexcept -> const element&
{
    return _e;
}

template<typename V>
constexpr auto iterator<V>::operator++() noexcept -> iterator&
{
    _e._first = _e._second;
    ++_e._second;
    return *this;
}

template<typename V>
constexpr bool iterator<V>::operator==(sentinel) const noexcept
{
    return _e._second == _end;
}

template<typename R>
constexpr view<R>::view(R&& r) noexcept(std::is_nothrow_constructible_v<base_type, R>) : _base{floormat::forward<R>(r)} {}

template<typename R>
constexpr auto view<R>::begin() noexcept -> iterator<base_type>
{
    return iterator<base_type>{_base};
}

template<typename R>
constexpr auto view<R>::begin() const noexcept -> iterator<const base_type>
{
    return iterator<const base_type>{_base};
}

template<typename R>
constexpr sentinel view<R>::end() const noexcept
{
    return {};
}

} // namespace floormat::detail::pairwise

namespace floormat {

// Each pair of consecutive elements (r[i], r[i + 1]), as C++23 std::views::pairwise.
template<typename R> requires range<R>
constexpr detail::pairwise::view<R> pairwise(R&& r) noexcept(std::is_nothrow_constructible_v<detail::pairwise::view<R>, R>)
{
    return detail::pairwise::view<R>{floormat::forward<R>(r)};
}

} // namespace floormat
