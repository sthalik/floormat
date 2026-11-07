#pragma once
#include "range-access.hpp"
#include "assert.hpp"
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail_zip {

template<usize... Is> struct index_sequence {};
template<usize N, usize... Is> struct make_index_sequence_ : make_index_sequence_<N-1, N-1, Is...> {};
template<usize... Is> struct make_index_sequence_<0, Is...> { using type = index_sequence<Is...>; };
template<usize N> using make_index_sequence = typename make_index_sequence_<N>::type;

template<usize I, typename T> struct leaf { T value; };

template<typename Seq, typename... Ts> struct tuple;
template<usize... Is, typename... Ts> struct tuple<index_sequence<Is...>, Ts...> : leaf<Is, Ts>... {};
template<typename... Ts> using tuple_t = tuple<make_index_sequence<sizeof...(Ts)>, Ts...>;

template<usize I, typename T>
constexpr T& get_leaf(leaf<I, T>& l) noexcept
{
    return l.value;
}

template<usize I, typename T>
constexpr const T& get_leaf(const leaf<I, T>& l) noexcept
{
    return l.value;
}

template<usize I, typename Tuple> using leaf_t = std::remove_cvref_t<decltype(get_leaf<I>(detail_range::declval<Tuple&>()))>;

template<typename T, typename...> struct first_ { using type = T; };
template<typename... Ts> using first_type = typename first_<Ts...>::type;

template<typename It>
struct component
{
    using reference = decltype(*detail_range::declval<It&>());
    static_assert(!std::is_rvalue_reference_v<reference>, "zip() needs ranges of lvalues or prvalues");
    static constexpr bool is_lvalue = std::is_lvalue_reference_v<reference>;
    using value_type = std::remove_reference_t<reference>;
    using ref = std::conditional_t<is_lvalue, value_type&, value_type>;
    using const_ref = std::conditional_t<is_lvalue, const value_type&, value_type>;
    using copy = std::conditional_t<is_lvalue, std::remove_cv_t<value_type>, value_type>;
};

template<typename S, typename I>
concept sized_sentinel_for = requires(const S& s, const I& i) { static_cast<usize>(s - i); };

template<typename It>
concept subscriptable = requires(const It& it, usize n) { it[n]; };

template<typename... Vs>
constexpr bool random_access =
    sized_sentinel_for<detail_range::end_t<first_type<Vs...>>, detail_range::begin_t<first_type<Vs...>>> &&
    (subscriptable<detail_range::begin_t<Vs>> && ...);

// GCC 16 drops the C++26 zero-fill of a std::array returned by value only when the loop subscripts the array itself.
// On MSVC, indexing through the view made is_passable_() call detail::is_passable out of line.
#if defined _MSC_VER && !defined __clang__
template<bool Index, typename V> constexpr bool by_range = false;
#else
template<bool Index, typename V> constexpr bool by_range = Index && detail_range::static_extent<std::remove_cv_t<V>> != 0;
#endif
template<bool Index, typename V> using current_t = std::conditional_t<by_range<Index, V>, V*, detail_range::begin_t<V>>;

template<typename V>
constexpr usize length(V& v) noexcept
{
    return static_cast<usize>(detail_range::end_of(v) - detail_range::begin_of(v));
}

template<typename V, typename... Vs>
constexpr bool same_length(V& v, Vs&... vs) noexcept
{
    const usize n = length(v);
    return (... && (length(vs) == n));
}

template<bool Index> struct index_base {};
template<> struct index_base<true> { usize _i = 0; };

template<bool Index, typename... Vs> class element;
template<typename... Vs> class iterator;
template<typename... Vs> class sentinel;
template<typename... Rs> class view;

template<bool Index, typename... Vs>
class element : index_base<Index>
{
    template<typename...> friend class iterator;
    using tuple_type = tuple_t<current_t<Index, Vs>...>;

    tuple_type _current;

    constexpr explicit element(Vs&... vs) noexcept;
    template<usize N, typename Self> static constexpr decltype(auto) deref(Self& self) noexcept;

public:
    template<usize N> using component_t = component<leaf_t<N, tuple_t<detail_range::begin_t<Vs>...>>>;

    template<usize N> constexpr auto get() & noexcept -> typename component_t<N>::ref;
    template<usize N> constexpr auto get() const& noexcept -> typename component_t<N>::const_ref;
    template<usize N> constexpr auto get() && noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy;
    template<usize N> constexpr auto get() const&& noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy;
};

template<typename... Vs>
class sentinel
{
    template<typename...> friend class iterator;
    template<typename...> friend class view;
    using V = first_type<Vs...>;
    using end_type = std::conditional_t<random_access<Vs...>, usize, detail_range::end_t<V>>;

    end_type _end;

    static constexpr end_type end_of(V& v) noexcept;
    constexpr explicit sentinel(V& v) noexcept;
};

// operator* returns the element stored here, so the reference dies with the iterator.
template<typename... Vs>
class iterator
{
    template<typename...> friend class view;
    using element_type = element<random_access<Vs...>, Vs...>;

    element_type _e;

    constexpr explicit iterator(Vs&... vs) noexcept;

public:
    constexpr element_type& operator*() noexcept;
    constexpr const element_type& operator*() const noexcept;
    constexpr iterator& operator++() noexcept;
    constexpr bool operator==(const sentinel<Vs...>& s) const noexcept;
};

template<typename... Rs>
class view
{
    static_assert(sizeof...(Rs) >= 2, "zip() needs at least two ranges");

    tuple_t<detail_range::all_t<Rs>...> _bases;

public:
    constexpr explicit view(Rs&&... rs) noexcept((std::is_nothrow_constructible_v<detail_range::all_t<Rs>, Rs> && ...));

    constexpr iterator<detail_range::all_t<Rs>...> begin() noexcept;
    constexpr iterator<const detail_range::all_t<Rs>...> begin() const noexcept;
    constexpr sentinel<detail_range::all_t<Rs>...> end() noexcept;
    constexpr sentinel<const detail_range::all_t<Rs>...> end() const noexcept;
};

template<bool Index, typename... Vs>
constexpr element<Index, Vs...>::element(Vs&... vs) noexcept : _current{ {[&]() -> current_t<Index, Vs> {
    if constexpr (by_range<Index, Vs>)
        return &vs;
    else
        return detail_range::begin_of(vs);
}()}... } {}

template<bool Index, typename... Vs>
template<usize N, typename Self>
constexpr decltype(auto) element<Index, Vs...>::deref(Self& self) noexcept
{
    if constexpr (by_range<Index, leaf_t<N, tuple_t<Vs...>>>)
        return get_leaf<N>(self._current)->base()[self._i];
    else if constexpr (Index && std::is_pointer_v<leaf_t<N, tuple_type>>)
        return get_leaf<N>(self._current)[self._i];
    // operator[] of a class iterator takes its signed difference_type.
    else if constexpr (Index)
        return get_leaf<N>(self._current)[static_cast<isize>(self._i)];
    else
        return *get_leaf<N>(self._current);
}

template<bool Index, typename... Vs>
template<usize N>
constexpr auto element<Index, Vs...>::get() & noexcept -> typename component_t<N>::ref
{
    return deref<N>(*this);
}

template<bool Index, typename... Vs>
template<usize N>
constexpr auto element<Index, Vs...>::get() const& noexcept -> typename component_t<N>::const_ref
{
    return deref<N>(*this);
}

template<bool Index, typename... Vs>
template<usize N>
constexpr auto element<Index, Vs...>::get() && noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy
{
    return deref<N>(*this);
}

template<bool Index, typename... Vs>
template<usize N>
constexpr auto element<Index, Vs...>::get() const&& noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy
{
    return deref<N>(*this);
}

template<typename... Vs>
constexpr auto sentinel<Vs...>::end_of(V& v) noexcept -> end_type
{
    if constexpr (random_access<Vs...>)
        return length(v);
    else
        return detail_range::end_of(v);
}

template<typename... Vs>
constexpr sentinel<Vs...>::sentinel(V& v) noexcept : _end{end_of(v)} {}

template<typename... Vs>
constexpr iterator<Vs...>::iterator(Vs&... vs) noexcept : _e{vs...}
{
    if constexpr ((sized_sentinel_for<detail_range::end_t<Vs>, detail_range::begin_t<Vs>> && ...))
        fm_debug3_assert(same_length(vs...));
}

template<typename... Vs>
constexpr auto iterator<Vs...>::operator*() noexcept -> element_type&
{
    return _e;
}

template<typename... Vs>
constexpr auto iterator<Vs...>::operator*() const noexcept -> const element_type&
{
    return _e;
}

template<typename... Vs>
constexpr auto iterator<Vs...>::operator++() noexcept -> iterator&
{
    if constexpr (random_access<Vs...>)
        ++_e._i;
    else
        [this]<usize... Is>(index_sequence<Is...>) { (++get_leaf<Is>(_e._current), ...); }(make_index_sequence<sizeof...(Vs)>{});
    return *this;
}

template<typename... Vs>
constexpr bool iterator<Vs...>::operator==(const sentinel<Vs...>& s) const noexcept
{
    if constexpr (random_access<Vs...>)
        return _e._i == s._end;
    else
        return get_leaf<0>(_e._current) == s._end;
}

template<typename... Rs>
constexpr view<Rs...>::view(Rs&&... rs) noexcept((std::is_nothrow_constructible_v<detail_range::all_t<Rs>, Rs> && ...)) :
    _bases{ {detail_range::all_t<Rs>{floormat::forward<Rs>(rs)}}... }
{}

template<typename... Rs>
constexpr auto view<Rs...>::begin() noexcept -> iterator<detail_range::all_t<Rs>...>
{
    return [this]<usize... Is>(index_sequence<Is...>) {
        return iterator<detail_range::all_t<Rs>...>{get_leaf<Is>(_bases)...};
    }(make_index_sequence<sizeof...(Rs)>{});
}

template<typename... Rs>
constexpr auto view<Rs...>::begin() const noexcept -> iterator<const detail_range::all_t<Rs>...>
{
    return [this]<usize... Is>(index_sequence<Is...>) {
        return iterator<const detail_range::all_t<Rs>...>{get_leaf<Is>(_bases)...};
    }(make_index_sequence<sizeof...(Rs)>{});
}

template<typename... Rs>
constexpr auto view<Rs...>::end() noexcept -> sentinel<detail_range::all_t<Rs>...>
{
    return sentinel<detail_range::all_t<Rs>...>{get_leaf<0>(_bases)};
}

template<typename... Rs>
constexpr auto view<Rs...>::end() const noexcept -> sentinel<const detail_range::all_t<Rs>...>
{
    return sentinel<const detail_range::all_t<Rs>...>{get_leaf<0>(_bases)};
}

} // namespace floormat::detail_zip

namespace floormat {

// Iterates as many times as the first range has elements. Only Debug builds check that the others are as long.
template<typename... Rs>
constexpr detail_zip::view<Rs...> zip(Rs&&... rs) noexcept(std::is_nothrow_constructible_v<detail_zip::view<Rs...>, Rs...>)
{
    return detail_zip::view<Rs...>{floormat::forward<Rs>(rs)...};
}

} // namespace floormat

namespace std {

template<bool Index, typename... Vs>
struct tuple_size<floormat::detail_zip::element<Index, Vs...>> : integral_constant<size_t, sizeof...(Vs)> {};
template<bool Index, typename... Vs>
struct tuple_size<const floormat::detail_zip::element<Index, Vs...>> : integral_constant<size_t, sizeof...(Vs)> {};

template<size_t I, bool Index, typename... Vs>
struct tuple_element<I, floormat::detail_zip::element<Index, Vs...>> { using type = typename floormat::detail_zip::element<Index, Vs...>::template component_t<I>::value_type; };
template<size_t I, bool Index, typename... Vs>
struct tuple_element<I, const floormat::detail_zip::element<Index, Vs...>> { using type = const typename tuple_element<I, floormat::detail_zip::element<Index, Vs...>>::type; };

} // namespace std
