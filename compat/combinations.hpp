#pragma once
#include "range-access.hpp"
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail_combinations {

template<typename V> class element;
template<typename V> class iterator;
template<typename R> class view;

struct sentinel {};

template<typename V>
class element
{
    template<typename> friend class iterator;
    using It = detail_range::begin_t<V>;

    It _first, _second;

    constexpr element(It first, It second) noexcept;

public:
    using reference = decltype(*detail_range::declval<It&>());
    static_assert(std::is_lvalue_reference_v<reference>, "combinations() needs a range of lvalues");
    static_assert(std::is_copy_constructible_v<It>, "combinations() needs a range it can walk more than once");
    using value_type = std::remove_reference_t<reference>;
    using copy_type = std::remove_cv_t<value_type>;

    template<usize N> constexpr value_type& get() & noexcept;
    template<usize N> constexpr const value_type& get() const& noexcept;
    template<usize N> constexpr copy_type get() && noexcept(std::is_nothrow_copy_constructible_v<copy_type>);
    template<usize N> constexpr copy_type get() const&& noexcept(std::is_nothrow_copy_constructible_v<copy_type>);
};

// operator* returns the element stored here, so the reference dies with the iterator.
template<typename V>
class iterator
{
    template<typename> friend class view;

    element<V> _e;
    detail_range::end_t<V> _end;

    constexpr explicit iterator(V& v) noexcept;

public:
    constexpr element<V>& operator*() noexcept;
    constexpr const element<V>& operator*() const noexcept;
    constexpr iterator& operator++() noexcept;
    constexpr bool operator==(sentinel) const noexcept;
};

template<typename R>
class view
{
    using base_type = detail_range::all_t<R>;

    base_type _base;

public:
    constexpr explicit view(R&& r) noexcept(std::is_nothrow_constructible_v<base_type, R>);

    constexpr iterator<base_type> begin() noexcept;
    constexpr iterator<const base_type> begin() const noexcept;
    constexpr sentinel end() const noexcept;
};

template<typename V>
constexpr element<V>::element(It first, It second) noexcept : _first{floormat::move(first)}, _second{floormat::move(second)} {}

template<typename V>
template<usize N>
constexpr auto element<V>::get() & noexcept -> value_type&
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return *_first;
    else
        return *_second;
}

template<typename V>
template<usize N>
constexpr auto element<V>::get() const& noexcept -> const value_type&
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return *_first;
    else
        return *_second;
}

template<typename V>
template<usize N>
constexpr auto element<V>::get() && noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> copy_type
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return *_first;
    else
        return *_second;
}

template<typename V>
template<usize N>
constexpr auto element<V>::get() const&& noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> copy_type
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return *_first;
    else
        return *_second;
}

template<typename V>
constexpr iterator<V>::iterator(V& v) noexcept :
    _e{detail_range::begin_of(v), detail_range::begin_of(v)}, _end{detail_range::end_of(v)}
{
    if (_e._second != _end)
        ++_e._second;
}

template<typename V>
constexpr auto iterator<V>::operator*() noexcept -> element<V>&
{
    return _e;
}

template<typename V>
constexpr auto iterator<V>::operator*() const noexcept -> const element<V>&
{
    return _e;
}

template<typename V>
constexpr auto iterator<V>::operator++() noexcept -> iterator&
{
    ++_e._second;
    if (_e._second == _end)
    {
        ++_e._first;
        _e._second = _e._first;
        ++_e._second;
    }
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

} // namespace floormat::detail_combinations

namespace floormat {

// Every pair (r[i], r[j]) with i < j, ordered by i and then by j, as Python's itertools.combinations(r, 2).
template<typename R>
constexpr detail_combinations::view<R> combinations(R&& r) noexcept(std::is_nothrow_constructible_v<detail_combinations::view<R>, R>)
{
    return detail_combinations::view<R>{floormat::forward<R>(r)};
}

} // namespace floormat

namespace std {

template<typename V>
struct tuple_size<floormat::detail_combinations::element<V>> : integral_constant<size_t, 2> {};
template<typename V>
struct tuple_size<const floormat::detail_combinations::element<V>> : integral_constant<size_t, 2> {};

template<size_t I, typename V>
struct tuple_element<I, floormat::detail_combinations::element<V>> { using type = typename floormat::detail_combinations::element<V>::value_type; };
template<size_t I, typename V>
struct tuple_element<I, const floormat::detail_combinations::element<V>> { using type = const typename floormat::detail_combinations::element<V>::value_type; };

} // namespace std
