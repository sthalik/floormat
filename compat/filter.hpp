#pragma once
#include "range-access.hpp"
#include "find-if.hpp"
#include "defs.hpp"
#include <concepts>
#include <cr/Macros.h>

namespace floormat::detail::filter {

template<typename V, typename P> class iterator;
template<typename Pred, typename R> class view;

struct sentinel {};

template<typename V, typename P>
class iterator
{
    template<typename, typename> friend class view;

    iterator_t<V> _it;
    sentinel_t<V> _end;
    const P* _pred;

    constexpr iterator(V& v, const P& pred);

public:
    constexpr CORRADE_ALWAYS_INLINE decltype(auto) operator*() noexcept;
    constexpr CORRADE_ALWAYS_INLINE decltype(auto) operator*() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE iterator& operator++();
    constexpr CORRADE_ALWAYS_INLINE bool operator==(sentinel) const noexcept;
};

template<typename Pred, typename R>
class view
{
    using base_type = detail::range::all_t<R>;
    using predicate_type = std::remove_cvref_t<Pred>;
    using predicate_storage_type = std::conditional_t<std::is_lvalue_reference_v<Pred>,
                                                      const predicate_type*, predicate_type>;

    predicate_storage_type _pred;
    base_type _base;

public:
    constexpr view(Pred&& pred, R&& r) noexcept((std::is_lvalue_reference_v<Pred> ||
                                                 std::is_nothrow_constructible_v<predicate_type, Pred>) &&
                                                std::is_nothrow_constructible_v<base_type, R>);

    constexpr const predicate_type& pred() const noexcept;
    constexpr iterator<base_type, predicate_type> begin();
    constexpr iterator<const base_type, predicate_type> begin() const
        requires std::predicate<const predicate_type&, iterator_reference_t<iterator_t<const base_type>>>;
    constexpr sentinel end() const noexcept;
};

template<typename V, typename P>
constexpr iterator<V, P>::iterator(V& v, const P& pred) :
    _it{floormat::begin(v)}, _end{floormat::end(v)}, _pred{__builtin_addressof(pred)}
{
    _it = floormat::find_if(move(_it), _end, pred);
}

template<typename V, typename P>
constexpr decltype(auto) iterator<V, P>::operator*() noexcept
{
    return *_it;
}

template<typename V, typename P>
constexpr decltype(auto) iterator<V, P>::operator*() const noexcept
{
    return *_it;
}

template<typename V, typename P>
constexpr auto iterator<V, P>::operator++() -> iterator&
{
    _it = floormat::find_if(move(++_it), _end, *_pred);
    return *this;
}

template<typename V, typename P>
constexpr bool iterator<V, P>::operator==(sentinel) const noexcept
{
    return _it == _end;
}

template<typename Pred, typename R>
constexpr view<Pred, R>::view(Pred&& pred, R&& r)
    noexcept((std::is_lvalue_reference_v<Pred> || std::is_nothrow_constructible_v<predicate_type, Pred>) &&
             std::is_nothrow_constructible_v<base_type, R>) :
    _pred{[&]() -> predicate_storage_type {
        if constexpr (std::is_lvalue_reference_v<Pred>)
            return __builtin_addressof(pred);
        else
            return forward<Pred>(pred);
    }()},
    _base{forward<R>(r)}
{}

template<typename Pred, typename R>
constexpr auto view<Pred, R>::pred() const noexcept -> const predicate_type&
{
    if constexpr (std::is_lvalue_reference_v<Pred>)
        return *_pred;
    else
        return _pred;
}

template<typename Pred, typename R>
constexpr auto view<Pred, R>::begin() -> iterator<base_type, predicate_type>
{
    return iterator<base_type, predicate_type>{_base, pred()};
}

template<typename Pred, typename R>
constexpr auto view<Pred, R>::begin() const -> iterator<const base_type, predicate_type>
    requires std::predicate<const predicate_type&, iterator_reference_t<iterator_t<const base_type>>>
{
    return iterator<const base_type, predicate_type>{_base, pred()};
}

template<typename Pred, typename R>
constexpr sentinel view<Pred, R>::end() const noexcept
{
    return {};
}

} // namespace floormat::detail::filter

namespace floormat {

#ifdef __clang__
#pragma clang diagnostic push
#if __has_warning("-Wlifetime-safety-lifetimebound-violation")
#pragma clang diagnostic ignored "-Wlifetime-safety-lifetimebound-violation"
#endif
#endif
// Returns a view of the elements of r for which pred returns true. An lvalue r or pred must outlive the view.
template<typename Pred, typename R>
requires std::is_lvalue_reference_v<Pred> && (!std::is_function_v<std::remove_reference_t<Pred>>) && range<R> &&
         std::predicate<const std::remove_cvref_t<Pred>&, iterator_reference_t<iterator_t<detail::range::all_t<R>>>>
constexpr detail::filter::view<Pred, R> filter(Pred&& pred [[fm_lifetimebound]], R&& r)
    noexcept(std::is_nothrow_constructible_v<detail::filter::view<Pred, R>, Pred, R>)
{
    return detail::filter::view<Pred, R>{forward<Pred>(pred), forward<R>(r)};
}
#ifdef __clang__
#pragma clang diagnostic pop
#endif

template<typename Pred, typename R>
requires (!std::is_lvalue_reference_v<Pred>) && range<R> &&
         std::predicate<const std::remove_cvref_t<Pred>&, iterator_reference_t<iterator_t<detail::range::all_t<R>>>>
constexpr detail::filter::view<Pred, R> filter(Pred&& pred, R&& r)
    noexcept(std::is_nothrow_constructible_v<detail::filter::view<Pred, R>, Pred, R>)
{
    return detail::filter::view<Pred, R>{forward<Pred>(pred), forward<R>(r)};
}

template<typename F, typename R>
requires std::is_function_v<F> && range<R> &&
         std::predicate<F&, iterator_reference_t<iterator_t<detail::range::all_t<R>>>>
constexpr detail::filter::view<F*, R> filter(F& pred, R&& r)
    noexcept(std::is_nothrow_constructible_v<detail::filter::view<F*, R>, F*, R>)
{
    return detail::filter::view<F*, R>{&pred, forward<R>(r)};
}

} // namespace floormat
