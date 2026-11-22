#pragma once
#include "fwd/iterator.hpp"
#include <cr/Macros.h>
#include <compare>
#include <concepts>
#include <type_traits>

namespace floormat {

template<typename I>
class reverse_iterator
{
    I _current{};

public:
    using iterator_type = I;
    using iterator_concept = std::conditional_t<std::random_access_iterator<I>, std::random_access_iterator_tag,
                                                std::bidirectional_iterator_tag>;
    using iterator_category = std::conditional_t<
        std::derived_from<typename std::iterator_traits<I>::iterator_category, std::random_access_iterator_tag>,
        std::random_access_iterator_tag, typename std::iterator_traits<I>::iterator_category>;
    using value_type = std::iter_value_t<I>;
    using difference_type = std::iter_difference_t<I>;
    using pointer = std::iterator_traits<I>::pointer;
    using reference = std::iter_reference_t<I>;

    reverse_iterator() = default;
    constexpr CORRADE_ALWAYS_INLINE explicit reverse_iterator(I current);
    template<typename U> requires (!std::is_same_v<U, I> && std::convertible_to<const U&, I>)
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator(const reverse_iterator<U>& other);
    template<typename U>
        requires (!std::is_same_v<U, I> && std::convertible_to<const U&, I> && std::assignable_from<I&, const U&>)
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator& operator=(const reverse_iterator<U>& other);

    constexpr CORRADE_ALWAYS_INLINE I base() const;
    constexpr CORRADE_ALWAYS_INLINE reference operator*() const;
    constexpr CORRADE_ALWAYS_INLINE pointer operator->() const
        requires (std::is_pointer_v<I> || requires(const I i) { i.operator->(); });
    constexpr CORRADE_ALWAYS_INLINE reference operator[](difference_type n) const;
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator& operator++();
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator operator++(int);
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator& operator--();
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator operator--(int);
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator& operator+=(difference_type n);
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator& operator-=(difference_type n);
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator operator+(difference_type n) const;
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator operator-(difference_type n) const;
};

template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() == y.base() } -> std::convertible_to<bool>;
    }
constexpr CORRADE_ALWAYS_INLINE bool operator==(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y);
template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() != y.base() } -> std::convertible_to<bool>;
    }
constexpr CORRADE_ALWAYS_INLINE bool operator!=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y);
template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() > y.base() } -> std::convertible_to<bool>;
    }
constexpr CORRADE_ALWAYS_INLINE bool operator<(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y);
template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() < y.base() } -> std::convertible_to<bool>;
    }
constexpr CORRADE_ALWAYS_INLINE bool operator>(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y);
template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() >= y.base() } -> std::convertible_to<bool>;
    }
constexpr CORRADE_ALWAYS_INLINE bool operator<=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y);
template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() <= y.base() } -> std::convertible_to<bool>;
    }
constexpr CORRADE_ALWAYS_INLINE bool operator>=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y);
template<typename I1, std::three_way_comparable_with<I1> I2>
constexpr CORRADE_ALWAYS_INLINE std::compare_three_way_result_t<I1, I2> operator<=>(const reverse_iterator<I1>& x,
                                                                                    const reverse_iterator<I2>& y);

template<typename I1, typename I2>
constexpr CORRADE_ALWAYS_INLINE auto operator-(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
    -> decltype(y.base() - x.base());
template<typename I>
constexpr CORRADE_ALWAYS_INLINE reverse_iterator<I> operator+(std::iter_difference_t<I> n, const reverse_iterator<I>& x);

template<typename I>
constexpr CORRADE_ALWAYS_INLINE std::iter_rvalue_reference_t<I> iter_move(const reverse_iterator<I>& i)
    noexcept(std::is_nothrow_copy_constructible_v<I> && noexcept(std::ranges::iter_move(--std::declval<I&>())));
template<typename I, std::indirectly_swappable<I> I2>
constexpr CORRADE_ALWAYS_INLINE void iter_swap(const reverse_iterator<I>& x, const reverse_iterator<I2>& y)
    noexcept(std::is_nothrow_copy_constructible_v<I> && std::is_nothrow_copy_constructible_v<I2> &&
             noexcept(std::ranges::iter_swap(--std::declval<I&>(), --std::declval<I2&>())));

template<typename I> constexpr CORRADE_ALWAYS_INLINE reverse_iterator<I> make_reverse_iterator(I i);

} // namespace floormat

namespace std {

template<typename I1, typename I2> requires (!sized_sentinel_for<I1, I2>)
constexpr inline bool disable_sized_sentinel_for<floormat::reverse_iterator<I1>, floormat::reverse_iterator<I2>> = true;

} // namespace std

namespace floormat {

template<typename I> constexpr reverse_iterator<I>::reverse_iterator(I current): _current{current} {}

template<typename I>
template<typename U> requires (!std::is_same_v<U, I> && std::convertible_to<const U&, I>)
constexpr reverse_iterator<I>::reverse_iterator(const reverse_iterator<U>& other): _current{other.base()} {}

template<typename I>
template<typename U>
    requires (!std::is_same_v<U, I> && std::convertible_to<const U&, I> && std::assignable_from<I&, const U&>)
constexpr auto reverse_iterator<I>::operator=(const reverse_iterator<U>& other) -> reverse_iterator&
{
    _current = other.base();
    return *this;
}

template<typename I> constexpr I reverse_iterator<I>::base() const { return _current; }

template<typename I>
constexpr auto reverse_iterator<I>::operator*() const -> reference
{
    I tmp = _current;
    return *--tmp;
}

template<typename I>
constexpr auto reverse_iterator<I>::operator->() const -> pointer
    requires (std::is_pointer_v<I> || requires(const I i) { i.operator->(); })
{
    I tmp = _current;
    --tmp;
    if constexpr (std::is_pointer_v<I>)
        return tmp;
    else
        return tmp.operator->();
}

template<typename I>
constexpr auto reverse_iterator<I>::operator[](difference_type n) const -> reference
{
    return _current[-n - 1];
}

template<typename I>
constexpr auto reverse_iterator<I>::operator++() -> reverse_iterator&
{
    --_current;
    return *this;
}

template<typename I>
constexpr auto reverse_iterator<I>::operator++(int) -> reverse_iterator
{
    reverse_iterator previous = *this;
    --_current;
    return previous;
}

template<typename I>
constexpr auto reverse_iterator<I>::operator--() -> reverse_iterator&
{
    ++_current;
    return *this;
}

template<typename I>
constexpr auto reverse_iterator<I>::operator--(int) -> reverse_iterator
{
    reverse_iterator previous = *this;
    ++_current;
    return previous;
}

template<typename I>
constexpr auto reverse_iterator<I>::operator+=(difference_type n) -> reverse_iterator&
{
    _current -= n;
    return *this;
}

template<typename I>
constexpr auto reverse_iterator<I>::operator-=(difference_type n) -> reverse_iterator&
{
    _current += n;
    return *this;
}

template<typename I>
constexpr auto reverse_iterator<I>::operator+(difference_type n) const -> reverse_iterator
{
    return reverse_iterator{_current - n};
}

template<typename I>
constexpr auto reverse_iterator<I>::operator-(difference_type n) const -> reverse_iterator
{
    return reverse_iterator{_current + n};
}

template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() == y.base() } -> std::convertible_to<bool>;
    }
constexpr bool operator==(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() == y.base();
}

template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() != y.base() } -> std::convertible_to<bool>;
    }
constexpr bool operator!=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() != y.base();
}

template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() > y.base() } -> std::convertible_to<bool>;
    }
constexpr bool operator<(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() > y.base();
}

template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() < y.base() } -> std::convertible_to<bool>;
    }
constexpr bool operator>(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() < y.base();
}

template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() >= y.base() } -> std::convertible_to<bool>;
    }
constexpr bool operator<=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() >= y.base();
}

template<typename I1, typename I2>
    requires requires(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) {
        { x.base() <= y.base() } -> std::convertible_to<bool>;
    }
constexpr bool operator>=(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y)
{
    return x.base() <= y.base();
}

template<typename I1, std::three_way_comparable_with<I1> I2>
constexpr std::compare_three_way_result_t<I1, I2> operator<=>(const reverse_iterator<I1>& x,
                                                             const reverse_iterator<I2>& y)
{
    return y.base() <=> x.base();
}

template<typename I1, typename I2>
constexpr auto operator-(const reverse_iterator<I1>& x, const reverse_iterator<I2>& y) -> decltype(y.base() - x.base())
{
    return y.base() - x.base();
}

template<typename I>
constexpr reverse_iterator<I> operator+(std::iter_difference_t<I> n, const reverse_iterator<I>& x)
{
    return reverse_iterator<I>{x.base() - n};
}

template<typename I>
constexpr std::iter_rvalue_reference_t<I> iter_move(const reverse_iterator<I>& i)
    noexcept(std::is_nothrow_copy_constructible_v<I> && noexcept(std::ranges::iter_move(--std::declval<I&>())))
{
    I tmp = i.base();
    return std::ranges::iter_move(--tmp);
}

template<typename I, std::indirectly_swappable<I> I2>
constexpr void iter_swap(const reverse_iterator<I>& x, const reverse_iterator<I2>& y)
    noexcept(std::is_nothrow_copy_constructible_v<I> && std::is_nothrow_copy_constructible_v<I2> &&
             noexcept(std::ranges::iter_swap(--std::declval<I&>(), --std::declval<I2&>())))
{
    I xtmp = x.base();
    I2 ytmp = y.base();
    std::ranges::iter_swap(--xtmp, --ytmp);
}

template<typename I> constexpr reverse_iterator<I> make_reverse_iterator(I i) { return reverse_iterator<I>{i}; }

} // namespace floormat
