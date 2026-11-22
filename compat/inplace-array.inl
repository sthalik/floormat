#pragma once
#include "inplace-array.hpp"
#include "erase-if.hpp"
#include "reverse-iterator.hpp"
#include <cr/Optional.h>

namespace floormat::detail::inplace_array_impl {

template<typename T>
constexpr synth_three_way_result<T> synth_three_way(const T& a, const T& b)
{
    if constexpr (std::three_way_comparable<T>)
        return a <=> b;
    else
        return a < b ? std::weak_ordering::less : b < a ? std::weak_ordering::greater : std::weak_ordering::equivalent;
}

template<typename T>
constexpr void reverse(T* first, T* last)
{
    for (; first != last && first != --last; ++first)
        floormat::swap(*first, *last);
}

template<typename T>
constexpr void rotate(T* first, T* middle, T* last)
{
    if (first != middle && middle != last)
    {
        reverse(first, middle);
        reverse(middle, last);
        reverse(first, last);
    }
}

template<typename T>
constexpr bool is_pointer_in_range(const T* first, const T* last, const T* p)
{
    if consteval
    {
        for (; first != last; ++first)
            if (p == first)
                return true;
        return false;
    }
    else
    {
        return (uptr)p - (uptr)first < (uptr)last - (uptr)first;
    }
}

} // namespace floormat::detail::inplace_array_impl

namespace floormat {

template<typename T, usize N>
constexpr void inplace_array<T, N>::assign(u32 count, const T& value)
{
    fm_assert(count <= N);
    clear();
    while (size() < count)
        unchecked_emplace_back(value);
}

template<typename T, usize N>
template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
constexpr void inplace_array<T, N>::assign(I first, S last)
{
    clear();
    for (; first != last; ++first)
        emplace_back(*first);
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::assign(std::initializer_list<T> list)
{
    *this = list;
}

template<typename T, usize N>
template<typename R> requires range<R>
constexpr void inplace_array<T, N>::assign_range(R&& r)
{
    assign(floormat::begin(r), floormat::end(r));
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::at(u32 i)
{
    fm_assert(i < size());
    return data()[i];
}

template<typename T, usize N>
constexpr const T& inplace_array<T, N>::at(u32 i) const
{
    fm_assert(i < size());
    return data()[i];
}

template<typename T, usize N> constexpr auto inplace_array<T, N>::rbegin() noexcept -> reverse_iterator { return reverse_iterator{end()}; }
template<typename T, usize N> constexpr auto inplace_array<T, N>::rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator{end()}; }
template<typename T, usize N> constexpr auto inplace_array<T, N>::rend() noexcept -> reverse_iterator { return reverse_iterator{begin()}; }
template<typename T, usize N> constexpr auto inplace_array<T, N>::rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator{begin()}; }
template<typename T, usize N> constexpr auto inplace_array<T, N>::crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
template<typename T, usize N> constexpr auto inplace_array<T, N>::crend() const noexcept -> const_reverse_iterator { return rend(); }

template<typename T, usize N>
template<typename R> requires range<R>
constexpr void inplace_array<T, N>::append_range(R&& r)
{
    const auto last = floormat::end(r);
    for (auto it = floormat::begin(r); it != last; ++it)
        emplace_back(*it);
}

template<typename T, usize N>
template<typename... Args>
constexpr Optional<T&> inplace_array<T, N>::try_emplace_back(Args&&... args)
{
    if (size() < capacity())
        return unchecked_emplace_back(forward<Args>(args)...);
    else
        return {};
}

template<typename T, usize N>
constexpr Optional<T&> inplace_array<T, N>::try_push_back(const T& value)
{
    return try_emplace_back(value);
}

template<typename T, usize N>
constexpr Optional<T&> inplace_array<T, N>::try_push_back(T&& value)
{
    return try_emplace_back(move(value));
}

template<typename T, usize N>
constexpr Optional<T&> inplace_array<T, N>::try_push_back(NoInitT)
{
    if (size() < capacity())
        return unchecked_push_back(NoInit);
    else
        return {};
}

template<typename T, usize N>
template<typename... Args>
constexpr T* inplace_array<T, N>::emplace(const T* pos, Args&&... args)
{
    if constexpr (sizeof...(Args) == 1 && (... && std::is_same_v<std::remove_cvref_t<Args>, T>))
        return insert(pos, forward<Args>(args)...);
    else if (pos == cend())
        return __builtin_addressof(emplace_back(forward<Args>(args)...));
    else
        return construct_at(insert(pos, NoInit), forward<Args>(args)...);
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::insert(const T* pos, const T& value)
{
    return insert(pos, 1u, value);
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::insert(const T* pos, T&& value)
{
    if (pos == cend())
        return __builtin_addressof(emplace_back(move(value)));
    else
    {
        T* src = __builtin_addressof(value);
        if (detail::inplace_array_impl::is_pointer_in_range(pos, cend(), src)) [[unlikely]]
            src += 1;
        return construct_at(insert(pos, NoInit), move(*src));
    }
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::insert(const T* pos, NoInitT)
{
    return insert(pos, NoInit, 1);
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::insert(const T* pos, u32 count, const T& value)
{
    fm_assert(count <= capacity() - size());
    if (pos == cend())
    {
        const u32 old_size = size();
        while (size() < old_size + count)
            unchecked_emplace_back(value);
        return begin() + old_size;
    }
    else
    {
        const T* src = __builtin_addressof(value);
        if (detail::inplace_array_impl::is_pointer_in_range(pos, cend(), src)) [[unlikely]]
            src += count;
        T* const first = insert(pos, NoInit, count);
        for (u32 i = 0; i < count; i++)
            construct_at(first + i, *src);
        return first;
    }
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::insert(const T* pos, NoInitT, u32 count)
{
    const auto i = (u32)(pos - cbegin());
    const u32 old_size = size();
    fm_debug_assert(i <= old_size);
    fm_assert(count <= capacity() - old_size);
    resize(NoInit, old_size + count);
    T* const first = begin() + i;
    T* const old_last = begin() + old_size;
    T* src = old_last;
    T* dst = end();
    while (src != first && dst != old_last)
        construct_at(--dst, move(*--src));
    while (src != first)
        *--dst = move(*--src);
    if constexpr (!std::is_trivially_destructible_v<T>)
        for (T* p = first; p != old_last && p != first + count; ++p)
            destroy_at(p);
    return first;
}

template<typename T, usize N>
template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
constexpr T* inplace_array<T, N>::insert(const T* pos, I first, S last)
{
    const auto i = (u32)(pos - cbegin());
    fm_debug_assert(i <= size());
    const u32 old_size = size();
    for (; first != last; ++first)
        emplace_back(*first);
    detail::inplace_array_impl::rotate(begin() + i, begin() + old_size, end());
    return begin() + i;
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::insert(const T* pos, std::initializer_list<T> list)
{
    fm_assert(list.size() <= capacity() - size());
    if (pos == cend())
    {
        const u32 old_size = size();
        for (const T& x : list)
            unchecked_emplace_back(x);
        return begin() + old_size;
    }
    else
    {
        T* const first = insert(pos, NoInit, (u32)list.size());
        T* dst = first;
        for (const T& x : list)
            construct_at(dst++, x);
        return first;
    }
}

template<typename T, usize N>
template<typename R> requires range<R>
constexpr T* inplace_array<T, N>::insert_range(const T* pos, R&& r)
{
    return insert(pos, floormat::begin(r), floormat::end(r));
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::swap(inplace_array& other)
    noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>)
{
    if (this != &other)
    {
        inplace_array& shorter = size() <= other.size() ? *this : other;
        inplace_array& longer = size() <= other.size() ? other : *this;
        const u32 n = shorter.size();
        for (u32 i = 0; i < n; i++)
            floormat::swap(shorter[i], longer[i]);
        for (u32 i = n; i < longer.size(); i++)
            shorter.unchecked_emplace_back(move(longer[i]));
        longer.erase(longer.begin() + n, longer.end());
    }
}

template<typename T>
constexpr void inplace_array<T, 0>::assign(u32 count, const T&)
{
    fm_assert(count == 0);
}

template<typename T>
template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
constexpr void inplace_array<T, 0>::assign(I first, S last)
{
    fm_assert(!(first != last));
}

template<typename T>
constexpr void inplace_array<T, 0>::assign(std::initializer_list<T> list)
{
    fm_assert(list.size() == 0);
}

template<typename T>
template<typename R> requires range<R>
constexpr void inplace_array<T, 0>::assign_range(R&& r)
{
    assign(floormat::begin(r), floormat::end(r));
}

template<typename T>
constexpr T& inplace_array<T, 0>::at(u32 i)
{
    fm_assert(i < size());
    return data()[i];
}

template<typename T>
constexpr const T& inplace_array<T, 0>::at(u32 i) const
{
    fm_assert(i < size());
    return data()[i];
}

template<typename T> constexpr auto inplace_array<T, 0>::rbegin() noexcept -> reverse_iterator { return reverse_iterator{end()}; }
template<typename T> constexpr auto inplace_array<T, 0>::rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator{end()}; }
template<typename T> constexpr auto inplace_array<T, 0>::rend() noexcept -> reverse_iterator { return reverse_iterator{begin()}; }
template<typename T> constexpr auto inplace_array<T, 0>::rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator{begin()}; }
template<typename T> constexpr auto inplace_array<T, 0>::crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
template<typename T> constexpr auto inplace_array<T, 0>::crend() const noexcept -> const_reverse_iterator { return rend(); }

template<typename T>
template<typename R> requires range<R>
constexpr void inplace_array<T, 0>::append_range(R&& r)
{
    fm_assert(!(floormat::begin(r) != floormat::end(r)));
}

template<typename T>
template<typename... Args>
constexpr Optional<T&> inplace_array<T, 0>::try_emplace_back(Args&&...)
{
    return {};
}

template<typename T> constexpr Optional<T&> inplace_array<T, 0>::try_push_back(const T&) { return {}; }
template<typename T> constexpr Optional<T&> inplace_array<T, 0>::try_push_back(T&&) { return {}; }
template<typename T> constexpr Optional<T&> inplace_array<T, 0>::try_push_back(NoInitT) { return {}; }

template<typename T>
template<typename... Args>
constexpr T* inplace_array<T, 0>::emplace(const T* pos, Args&&... args)
{
    fm_debug_assert(pos == cbegin());
    emplace_back(forward<Args>(args)...);
    return data();
}

template<typename T>
constexpr T* inplace_array<T, 0>::insert(const T* pos, const T& value)
{
    return emplace(pos, value);
}

template<typename T>
constexpr T* inplace_array<T, 0>::insert(const T* pos, T&& value)
{
    return emplace(pos, move(value));
}

template<typename T>
constexpr T* inplace_array<T, 0>::insert(const T* pos, NoInitT)
{
    fm_debug_assert(pos == cbegin());
    push_back(NoInit);
    return data();
}

template<typename T>
constexpr T* inplace_array<T, 0>::insert(const T* pos, u32 count, const T&)
{
    fm_debug_assert(pos == cbegin());
    fm_assert(count == 0);
    return data();
}

template<typename T>
constexpr T* inplace_array<T, 0>::insert(const T* pos, NoInitT, u32 count)
{
    fm_debug_assert(pos == cbegin());
    fm_assert(count == 0);
    return data();
}

template<typename T>
template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
constexpr T* inplace_array<T, 0>::insert(const T* pos, I first, S last)
{
    fm_debug_assert(pos == cbegin());
    fm_assert(!(first != last));
    return data();
}

template<typename T>
constexpr T* inplace_array<T, 0>::insert(const T* pos, std::initializer_list<T> list)
{
    return insert(pos, list.begin(), list.end());
}

template<typename T>
template<typename R> requires range<R>
constexpr T* inplace_array<T, 0>::insert_range(const T* pos, R&& r)
{
    return insert(pos, floormat::begin(r), floormat::end(r));
}

template<typename T>
constexpr void inplace_array<T, 0>::swap(inplace_array&) noexcept {}

template<typename T, usize N> requires detail::inplace_array_impl::less_than_comparable<T>
constexpr detail::inplace_array_impl::synth_three_way_result<T>
operator<=>(const inplace_array<T, N>& a, const inplace_array<T, N>& b)
{
    using R = detail::inplace_array_impl::synth_three_way_result<T>;
    const u32 n = a.size() < b.size() ? a.size() : b.size();
    for (u32 i = 0; i < n; i++)
        if (const R c = detail::inplace_array_impl::synth_three_way(a[i], b[i]); std::is_neq(c))
            return c;
    return a.size() <=> b.size();
}

template<typename T, usize N>
constexpr void swap(inplace_array<T, N>& a, inplace_array<T, N>& b) noexcept(noexcept(a.swap(b)))
{
    a.swap(b);
}

template<typename T, usize N, typename U>
constexpr u32 erase(inplace_array<T, N>& c, const U& value)
{
    return (u32)erase_if(c, [&value](const T& x) { return x == value; });
}

} // namespace floormat
