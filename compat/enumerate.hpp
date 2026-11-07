#pragma once
#include "range-access.hpp"
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail_enumerate {

// A sized range with operator[] ends its loop when the index reaches the size. Comparing an iterator with end
// would keep a second induction variable, which MSVC never merges with the index and Clang sometimes doesn't.
template<typename V>
concept indexed = detail_range::sized<V> && requires(const detail_range::begin_t<V>& it, usize i) { it[i]; };

// x86 addresses begin[index] in one instruction for element sizes 1, 2, 4 and 8. For other sizes the iterator
// advances as well, because MSVC would compute index * size at every access.
template<typename V, usize Size = sizeof(*detail_range::declval<detail_range::begin_t<V>&>())>
constexpr bool scaled_index = indexed<V> && (Size == 1 || Size == 2 || Size == 4 || Size == 8);

#if defined __GNUC__ && !defined __clang__
// When a loop ends on the index, GCC computes the address of an element of another size from the index at each
// step. Ending it on the iterator, as for a range without operator[], keeps the iterator as the address.
template<typename V> constexpr bool ends_on_index = scaled_index<V>;
#else
template<typename V> constexpr bool ends_on_index = indexed<V>;
#endif

template<typename V> class element;
template<typename V> class iterator;
template<typename R> class view;

// Ends the loop over an indexed range of N elements, or of size elements if N is 0.
template<usize N> struct sentinel { static constexpr usize size = N; };
template<> struct sentinel<0> { usize size; };

template<usize N, typename T> using get_t = std::conditional_t<N == 0, u32, T>;
template<typename V> using end_type = std::conditional_t<ends_on_index<V>, sentinel<detail_range::static_extent<std::remove_cv_t<V>>>,
                                                         detail_range::end_t<V>>;

template<typename V>
class element
{
    template<typename> friend class iterator;
    using It = detail_range::begin_t<V>;

    It _it;
    // usize, since a u32 index would be zero-extended wherever it addresses an element or meets the size.
    std::conditional_t<ends_on_index<V>, usize, u32> _index = 0;

    constexpr explicit element(It it) noexcept;
    constexpr decltype(auto) value() noexcept;
    constexpr decltype(auto) value() const noexcept;

public:
    using reference = decltype(*detail_range::declval<It&>());
    static_assert(std::is_lvalue_reference_v<reference>, "enumerate() needs a range of lvalues");
    using value_type = std::remove_reference_t<reference>;
    using copy_type = std::remove_cv_t<value_type>;

    template<usize N> constexpr auto get() & noexcept -> get_t<N, value_type&>;
    template<usize N> constexpr auto get() const& noexcept -> get_t<N, const value_type&>;
    template<usize N> constexpr auto get() && noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>;
    template<usize N> constexpr auto get() const&& noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>;
};

// operator* returns the element stored here, so the reference dies with the iterator.
template<typename V>
class iterator
{
    template<typename> friend class view;

    element<V> _e;

    constexpr explicit iterator(V& v) noexcept;

public:
    constexpr element<V>& operator*() noexcept;
    constexpr const element<V>& operator*() const noexcept;
    constexpr iterator& operator++() noexcept;
    constexpr bool operator==(const end_type<V>& end) const noexcept;
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
    constexpr end_type<base_type> end() noexcept;
    constexpr end_type<const base_type> end() const noexcept;
};

template<typename V>
constexpr element<V>::element(It it) noexcept : _it{floormat::move(it)} {}

template<typename V>
constexpr decltype(auto) element<V>::value() noexcept
{
    if constexpr (scaled_index<V> && std::is_pointer_v<It>)
        return _it[_index];
    // operator[] of a class iterator takes its signed difference_type.
    else if constexpr (scaled_index<V>)
        return _it[static_cast<isize>(_index)];
    else
        return *_it;
}

template<typename V>
constexpr decltype(auto) element<V>::value() const noexcept
{
    if constexpr (scaled_index<V> && std::is_pointer_v<It>)
        return _it[_index];
    else if constexpr (scaled_index<V>)
        return _it[static_cast<isize>(_index)];
    else
        return *_it;
}

template<typename V>
template<usize N>
constexpr auto element<V>::get() & noexcept -> get_t<N, value_type&>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return u32(_index);
    else
        return value();
}

template<typename V>
template<usize N>
constexpr auto element<V>::get() const& noexcept -> get_t<N, const value_type&>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return u32(_index);
    else
        return value();
}

template<typename V>
template<usize N>
constexpr auto element<V>::get() && noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return u32(_index);
    else
        return value();
}

template<typename V>
template<usize N>
constexpr auto element<V>::get() const&& noexcept(std::is_nothrow_copy_constructible_v<copy_type>) -> get_t<N, copy_type>
{
    static_assert(N < 2);
    if constexpr (N == 0)
        return u32(_index);
    else
        return value();
}

template<typename V>
constexpr iterator<V>::iterator(V& v) noexcept : _e{detail_range::begin_of(v)} {}

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
    if constexpr (!scaled_index<V>)
        ++_e._it;
    ++_e._index;
    return *this;
}

template<typename V>
constexpr bool iterator<V>::operator==(const end_type<V>& end) const noexcept
{
    // With ==, MSVC compares a copy of the index from before ++ with size - 1, which costs a mov at each step.
    if constexpr (ends_on_index<V>)
        return _e._index >= end.size;
    else
        return _e._it == end;
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
constexpr auto view<R>::end() noexcept -> end_type<base_type>
{
    if constexpr (!ends_on_index<base_type>)
        return detail_range::end_of(_base);
    else if constexpr (detail_range::static_extent<base_type> > 0)
        return {};
    else
        return {detail_range::size_of(_base)};
}

template<typename R>
constexpr auto view<R>::end() const noexcept -> end_type<const base_type>
{
    if constexpr (!ends_on_index<const base_type>)
        return detail_range::end_of(_base);
    else if constexpr (detail_range::static_extent<base_type> > 0)
        return {};
    else
        return {detail_range::size_of(_base)};
}

} // namespace floormat::detail_enumerate

namespace floormat {

template<typename R>
constexpr detail_enumerate::view<R> enumerate(R&& r) noexcept(std::is_nothrow_constructible_v<detail_enumerate::view<R>, R>)
{
    return detail_enumerate::view<R>{floormat::forward<R>(r)};
}

} // namespace floormat

namespace std {

template<typename V>
struct tuple_size<floormat::detail_enumerate::element<V>> : integral_constant<size_t, 2> {};
template<typename V>
struct tuple_size<const floormat::detail_enumerate::element<V>> : integral_constant<size_t, 2> {};

template<typename V>
struct tuple_element<0, floormat::detail_enumerate::element<V>> { using type = floormat::u32; };
template<typename V>
struct tuple_element<1, floormat::detail_enumerate::element<V>> { using type = typename floormat::detail_enumerate::element<V>::value_type; };
template<size_t I, typename V>
struct tuple_element<I, const floormat::detail_enumerate::element<V>> { using type = const typename tuple_element<I, floormat::detail_enumerate::element<V>>::type; };

} // namespace std
