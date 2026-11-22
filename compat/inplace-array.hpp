#pragma once
#include "inplace-array.h"

namespace floormat {

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(u32 count): inplace_array{}
{
    resize(count);
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(NoInitT, u32 count): inplace_array{}
{
    resize(NoInit, count);
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(u32 count, const T& value): inplace_array{}
{
    resize(count, value);
}

template<typename T, usize N>
template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
constexpr inplace_array<T, N>::inplace_array(I first, S last): inplace_array{}
{
    for (; first != last; ++first)
        emplace_back(*first);
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(std::initializer_list<T> list): inplace_array{}
{
    fm_assert(list.size() <= N);
    for (const T& x : list)
        unchecked_emplace_back(x);
}

template<typename T, usize N>
constexpr inplace_array<T, N>& inplace_array<T, N>::operator=(std::initializer_list<T> list)
{
    fm_assert(list.size() <= N);
    clear();
    for (const T& x : list)
        unchecked_emplace_back(x);
    return *this;
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::operator[](u32 i) noexcept
{
    fm_debug_assert(i < size());
    return data()[i];
}

template<typename T, usize N>
constexpr const T& inplace_array<T, N>::operator[](u32 i) const noexcept
{
    fm_debug_assert(i < size());
    return data()[i];
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::front() noexcept
{
    fm_debug_assert(!empty());
    return data()[0];
}

template<typename T, usize N>
constexpr const T& inplace_array<T, N>::front() const noexcept
{
    fm_debug_assert(!empty());
    return data()[0];
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::back() noexcept
{
    fm_debug_assert(!empty());
    return data()[size() - 1];
}

template<typename T, usize N>
constexpr const T& inplace_array<T, N>::back() const noexcept
{
    fm_debug_assert(!empty());
    return data()[size() - 1];
}

template<typename T, usize N> constexpr const T* inplace_array<T, N>::cbegin() const noexcept { return data(); }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::cend() const noexcept { return data() + size(); }
template<typename T, usize N> constexpr u32 inplace_array<T, N>::max_size() noexcept { return (u32)N; }

template<typename T, usize N>
constexpr void inplace_array<T, N>::resize(u32 count)
{
    const u32 old_size = size();
    resize(NoInit, count);
    for (u32 i = old_size; i < count; i++)
        construct_at(data() + i);
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::resize(NoInitT, u32 count)
{
    fm_assert(count <= N);
    if (count < size())
        erase(begin() + count, end());
    else if (count > size())
    {
        if (std::is_constant_evaluated() && empty())
            start_storage_lifetime();
        annotate_contiguous_container(size(), count);
        _size = (stored_size_type)count;
    }
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::resize(u32 count, const T& value)
{
    fm_assert(count <= N);
    if (count < size())
        erase(begin() + count, end());
    else
        while (size() < count)
            unchecked_emplace_back(value);
}

template<typename T, usize N>
template<typename... Args>
constexpr T& inplace_array<T, N>::emplace_back(Args&&... args)
{
    fm_assert(size() < capacity());
    return unchecked_emplace_back(forward<Args>(args)...);
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::push_back(const T& value)
{
    return emplace_back(value);
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::push_back(T&& value)
{
    return emplace_back(move(value));
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::push_back(NoInitT)
{
    fm_assert(size() < capacity());
    return unchecked_push_back(NoInit);
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::unchecked_push_back(const T& value)
{
    return unchecked_emplace_back(value);
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::unchecked_push_back(T&& value)
{
    return unchecked_emplace_back(move(value));
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::pop_back() noexcept
{
    fm_debug_assert(!empty());
    --_size;
    if constexpr (!std::is_trivially_destructible_v<T>)
        destroy_at(data() + size());
    annotate_contiguous_container(size() + 1, size());
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::erase(const T* pos)
{
    fm_debug_assert(pos >= cbegin() && pos < cend());
    return erase(pos, pos + 1);
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::erase(const T* first, const T* last)
{
    fm_debug_assert(cbegin() <= first && first <= last && last <= cend());
    const u32 old_size = size();
    T* const from = begin() + (first - cbegin());
    T* out = from;
    for (T* p = begin() + (last - cbegin()); p != end(); ++p, ++out)
        *out = move(*p);
    if constexpr (!std::is_trivially_destructible_v<T>)
        for (T* p = out; p != end(); ++p)
            destroy_at(p);
    _size = (stored_size_type)(out - begin());
    annotate_contiguous_container(old_size, size());
    return from;
}

template<typename T, usize N>
constexpr inplace_array<T, N>::operator ArrayView<T>() noexcept
{
    return {data(), size()};
}

template<typename T, usize N>
constexpr inplace_array<T, N>::operator ArrayView<const T>() const noexcept
{
    return {data(), size()};
}

template<typename T, usize N>
constexpr bool operator==(const inplace_array<T, N>& a, const inplace_array<T, N>& b)
{
    if (a.size() != b.size())
        return false;
    else
    {
        for (u32 i = 0; i < a.size(); i++)
            if (!(a[i] == b[i]))
                return false;
        return true;
    }
}

template<typename T, usize N>
constexpr void container_traits<inplace_array<T, N>>::remove_suffix(inplace_array<T, N>& c, usize count)
{
    fm_debug_assert(count <= c.size());
    c.erase(c.end() - count, c.end());
}

} // namespace floormat
