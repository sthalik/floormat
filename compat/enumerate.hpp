#pragma once
#include "compat/range-access.hpp"
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail_enumerate {

template<typename It> class element;
template<typename V> class iterator;
template<typename R> class view;

template<size_t N, typename T> using get_t = std::conditional_t<N == 0, uint32_t, T>;

template<typename It>
class element
{
    template<typename> friend class iterator;

    It _it;
    uint32_t _index = 0;

    constexpr explicit element(It it) noexcept;

public:
    using reference = decltype(*detail_range::declval<It&>());
    static_assert(std::is_lvalue_reference_v<reference>, "enumerate() needs a range of lvalues");
    using value_type = std::remove_reference_t<reference>;
    using copy_type = std::remove_cv_t<value_type>;

    template<size_t N> constexpr auto get() & noexcept -> get_t<N, value_type&>;
    template<size_t N> constexpr auto get() const& noexcept -> get_t<N, const value_type&>;
    template<size_t N> constexpr auto get() && noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>;
    template<size_t N> constexpr auto get() const&& noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>;
};

// operator* returns the element stored here, so the reference dies with the iterator.
template<typename V>
class iterator
{
    template<typename> friend class view;
    using It = detail_range::begin_t<V>;
    using End = detail_range::end_t<V>;

    element<It> _e;

    constexpr explicit iterator(V& v) noexcept;

public:
    constexpr element<It>& operator*() noexcept;
    constexpr const element<It>& operator*() const noexcept;
    constexpr iterator& operator++() noexcept;
    constexpr bool operator==(const End& end) const noexcept;
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
    constexpr detail_range::end_t<base_type> end() noexcept;
    constexpr detail_range::end_t<const base_type> end() const noexcept;
};

template<typename It>
constexpr element<It>::element(It it) noexcept : _it{move(it)} {}

template<typename It>
template<size_t N>
constexpr auto element<It>::get() & noexcept -> get_t<N, value_type&>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return _index;
    else
        return *_it;
}

template<typename It>
template<size_t N>
constexpr auto element<It>::get() const& noexcept -> get_t<N, const value_type&>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return _index;
    else
        return *_it;
}

template<typename It>
template<size_t N>
constexpr auto element<It>::get() && noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return _index;
    else
        return *_it;
}

template<typename It>
template<size_t N>
constexpr auto element<It>::get() const&& noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return _index;
    else
        return *_it;
}

template<typename V>
constexpr iterator<V>::iterator(V& v) noexcept : _e{detail_range::begin_of(v)} {}

template<typename V>
constexpr auto iterator<V>::operator*() noexcept -> element<It>&
{
    return _e;
}

template<typename V>
constexpr auto iterator<V>::operator*() const noexcept -> const element<It>&
{
    return _e;
}

template<typename V>
constexpr auto iterator<V>::operator++() noexcept -> iterator&
{
    ++_e._it;
    ++_e._index;
    return *this;
}

template<typename V>
constexpr bool iterator<V>::operator==(const End& end) const noexcept
{
    return _e._it == end;
}

template<typename R>
constexpr view<R>::view(R&& r) noexcept(std::is_nothrow_constructible_v<base_type, R>) : _base{forward<R>(r)} {}

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
constexpr auto view<R>::end() noexcept -> detail_range::end_t<base_type>
{
    return detail_range::end_of(_base);
}

template<typename R>
constexpr auto view<R>::end() const noexcept -> detail_range::end_t<const base_type>
{
    return detail_range::end_of(_base);
}

} // namespace floormat::detail_enumerate

namespace floormat {

template<typename R>
constexpr detail_enumerate::view<R> enumerate(R&& r) noexcept(std::is_nothrow_constructible_v<detail_enumerate::view<R>, R>)
{
    return detail_enumerate::view<R>{forward<R>(r)};
}

} // namespace floormat

namespace std {

template<typename It>
struct tuple_size<floormat::detail_enumerate::element<It>> : integral_constant<size_t, 2> {};
template<typename It>
struct tuple_size<const floormat::detail_enumerate::element<It>> : integral_constant<size_t, 2> {};

template<typename It>
struct tuple_element<0, floormat::detail_enumerate::element<It>> { using type = floormat::uint32_t; };
template<typename It>
struct tuple_element<1, floormat::detail_enumerate::element<It>> { using type = typename floormat::detail_enumerate::element<It>::value_type; };
template<size_t I, typename It>
struct tuple_element<I, const floormat::detail_enumerate::element<It>> { using type = const typename tuple_element<I, floormat::detail_enumerate::element<It>>::type; };

} // namespace std
