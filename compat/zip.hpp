#pragma once
#include "compat/range-access.hpp"
#include "compat/assert.hpp"
#include <cr/StlForwardTupleSizeElement.h>

namespace floormat::detail_zip {

template<size_t... Is> struct index_sequence {};
template<size_t N, size_t... Is> struct make_index_sequence_ : make_index_sequence_<N-1, N-1, Is...> {};
template<size_t... Is> struct make_index_sequence_<0, Is...> { using type = index_sequence<Is...>; };
template<size_t N> using make_index_sequence = typename make_index_sequence_<N>::type;

template<size_t I, typename T> struct leaf { T value; };

template<typename Seq, typename... Ts> struct tuple;
template<size_t... Is, typename... Ts> struct tuple<index_sequence<Is...>, Ts...> : leaf<Is, Ts>... {};
template<typename... Ts> using tuple_t = tuple<make_index_sequence<sizeof...(Ts)>, Ts...>;

template<size_t I, typename T>
constexpr T& get_leaf(leaf<I, T>& l) noexcept
{
    return l.value;
}

template<size_t I, typename T>
constexpr const T& get_leaf(const leaf<I, T>& l) noexcept
{
    return l.value;
}

template<size_t I, typename Tuple> using leaf_t = std::remove_cvref_t<decltype(get_leaf<I>(detail_range::declval<Tuple&>()))>;

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
concept sized_sentinel_for = requires(const S& s, const I& i) { static_cast<size_t>(s - i); };

template<typename V>
constexpr size_t length(V& v) noexcept
{
    return static_cast<size_t>(detail_range::end_of(v) - detail_range::begin_of(v));
}

template<typename V, typename... Vs>
constexpr bool same_length(V& v, Vs&... vs) noexcept
{
    const size_t n = length(v);
    return (... && (length(vs) == n));
}

template<typename... Its> class element;
template<typename... Vs> class iterator;
template<typename... Vs> class sentinel;
template<typename... Rs> class view;

template<typename... Its>
class element
{
    template<typename...> friend class iterator;
    using tuple_type = tuple_t<Its...>;

    tuple_type _its;

    constexpr explicit element(Its... its) noexcept;
    template<size_t... Is> constexpr void advance(index_sequence<Is...>) noexcept;
    template<typename... Ends, size_t... Is> constexpr bool at_end(const tuple<index_sequence<Is...>, Ends...>& ends) const noexcept;

public:
    template<size_t N> using component_t = component<leaf_t<N, tuple_type>>;

    template<size_t N> constexpr auto get() & noexcept -> typename component_t<N>::ref;
    template<size_t N> constexpr auto get() const& noexcept -> typename component_t<N>::const_ref;
    template<size_t N> constexpr auto get() && noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy;
    template<size_t N> constexpr auto get() const&& noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy;
};

template<typename... Vs>
class sentinel
{
    template<typename...> friend class iterator;
    template<typename...> friend class view;

    tuple_t<detail_range::end_t<Vs>...> _ends;

    constexpr explicit sentinel(Vs&... vs) noexcept;
};

// operator* returns the element stored here, so the reference dies with the iterator.
template<typename... Vs>
class iterator
{
    template<typename...> friend class view;
    using element_type = element<detail_range::begin_t<Vs>...>;

    element_type _e;

    constexpr explicit iterator(Vs&... vs) noexcept;

public:
    constexpr element_type& operator*() noexcept;
    constexpr const element_type& operator*() const noexcept;
    constexpr iterator& operator++() noexcept;
    constexpr bool operator==(const sentinel<Vs...>& s) const noexcept;
};

template<typename... Rs>
constexpr bool nothrow_view_ctor = (std::is_nothrow_constructible_v<detail_range::all_t<Rs>, Rs> && ...); // Workaround for C2382 in MSVC 19.51, fixed in 19.52.

template<typename... Rs>
class view
{
    static_assert(sizeof...(Rs) >= 2, "zip() needs at least two ranges");

    tuple_t<detail_range::all_t<Rs>...> _bases;

public:
    constexpr explicit view(Rs&&... rs) noexcept(nothrow_view_ctor<Rs...>);

    constexpr iterator<detail_range::all_t<Rs>...> begin() noexcept;
    constexpr iterator<const detail_range::all_t<Rs>...> begin() const noexcept;
    constexpr sentinel<detail_range::all_t<Rs>...> end() noexcept;
    constexpr sentinel<const detail_range::all_t<Rs>...> end() const noexcept;
};

template<typename... Its>
constexpr element<Its...>::element(Its... its) noexcept : _its{ {move(its)}... } {}

template<typename... Its>
template<size_t... Is>
constexpr void element<Its...>::advance(index_sequence<Is...>) noexcept
{
    (++get_leaf<Is>(_its), ...);
}

template<typename... Its>
template<typename... Ends, size_t... Is>
constexpr bool element<Its...>::at_end(const tuple<index_sequence<Is...>, Ends...>& ends) const noexcept
{
    if (!(... || (get_leaf<Is>(_its) == get_leaf<Is>(ends))))
        return false;
#ifndef FM_NO_DEBUG
    if (!(... && (get_leaf<Is>(_its) == get_leaf<Is>(ends)))) [[unlikely]]
        fm_abort("zip() ranges differ in length");
#endif
    return true;
}

template<typename... Its>
template<size_t N>
constexpr auto element<Its...>::get() & noexcept -> typename component_t<N>::ref
{
    return *get_leaf<N>(_its);
}

template<typename... Its>
template<size_t N>
constexpr auto element<Its...>::get() const& noexcept -> typename component_t<N>::const_ref
{
    return *get_leaf<N>(_its);
}

template<typename... Its>
template<size_t N>
constexpr auto element<Its...>::get() && noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy
{
    return *get_leaf<N>(_its);
}

template<typename... Its>
template<size_t N>
constexpr auto element<Its...>::get() const&& noexcept(std::is_nothrow_copy_constructible_v<typename component_t<N>::copy>) -> typename component_t<N>::copy
{
    return *get_leaf<N>(_its);
}

template<typename... Vs>
constexpr sentinel<Vs...>::sentinel(Vs&... vs) noexcept : _ends{ {detail_range::end_of(vs)}... } {}

template<typename... Vs>
constexpr iterator<Vs...>::iterator(Vs&... vs) noexcept : _e{detail_range::begin_of(vs)...}
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
    _e.advance(make_index_sequence<sizeof...(Vs)>{});
    return *this;
}

template<typename... Vs>
constexpr bool iterator<Vs...>::operator==(const sentinel<Vs...>& s) const noexcept
{
    return _e.at_end(s._ends);
}

template<typename... Rs>
constexpr view<Rs...>::view(Rs&&... rs) noexcept(nothrow_view_ctor<Rs...>) :
    _bases{ {detail_range::all_t<Rs>{forward<Rs>(rs)}}... }
{}

template<typename... Rs>
constexpr auto view<Rs...>::begin() noexcept -> iterator<detail_range::all_t<Rs>...>
{
    return [this]<size_t... Is>(index_sequence<Is...>) {
        return iterator<detail_range::all_t<Rs>...>{get_leaf<Is>(_bases)...};
    }(make_index_sequence<sizeof...(Rs)>{});
}

template<typename... Rs>
constexpr auto view<Rs...>::begin() const noexcept -> iterator<const detail_range::all_t<Rs>...>
{
    return [this]<size_t... Is>(index_sequence<Is...>) {
        return iterator<const detail_range::all_t<Rs>...>{get_leaf<Is>(_bases)...};
    }(make_index_sequence<sizeof...(Rs)>{});
}

template<typename... Rs>
constexpr auto view<Rs...>::end() noexcept -> sentinel<detail_range::all_t<Rs>...>
{
    return [this]<size_t... Is>(index_sequence<Is...>) {
        return sentinel<detail_range::all_t<Rs>...>{get_leaf<Is>(_bases)...};
    }(make_index_sequence<sizeof...(Rs)>{});
}

template<typename... Rs>
constexpr auto view<Rs...>::end() const noexcept -> sentinel<const detail_range::all_t<Rs>...>
{
    return [this]<size_t... Is>(index_sequence<Is...>) {
        return sentinel<const detail_range::all_t<Rs>...>{get_leaf<Is>(_bases)...};
    }(make_index_sequence<sizeof...(Rs)>{});
}

} // namespace floormat::detail_zip

namespace floormat {

// Stops at the first range that ends, and aborts unless every range ends there.
template<typename... Rs>
constexpr detail_zip::view<Rs...> zip(Rs&&... rs) noexcept(std::is_nothrow_constructible_v<detail_zip::view<Rs...>, Rs...>)
{
    return detail_zip::view<Rs...>{forward<Rs>(rs)...};
}

} // namespace floormat

namespace std {

template<typename... Its>
struct tuple_size<floormat::detail_zip::element<Its...>> : integral_constant<size_t, sizeof...(Its)> {};
template<typename... Its>
struct tuple_size<const floormat::detail_zip::element<Its...>> : integral_constant<size_t, sizeof...(Its)> {};

template<size_t I, typename... Its>
struct tuple_element<I, floormat::detail_zip::element<Its...>> { using type = typename floormat::detail_zip::element<Its...>::template component_t<I>::value_type; };
template<size_t I, typename... Its>
struct tuple_element<I, const floormat::detail_zip::element<Its...>> { using type = const typename tuple_element<I, floormat::detail_zip::element<Its...>>::type; };

} // namespace std
