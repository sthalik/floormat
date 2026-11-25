#pragma once
#include "sorted-string-map.h"
#include "construct-at.hpp"
#include "move.hpp"
#include <cstring>
#include <cr/Pair.h>

namespace floormat {

template<typename T>
template<typename P>
auto sorted_string_map<T>::insert(P&& value) -> Pair<iterator, bool>
    requires requires(P& p) { p.first(); p.second(); } || requires(P& p) { p.first; p.second; }
{
    if constexpr (requires(P& p) { p.first(); })
        return try_emplace(value.first(), forward<P>(value).second());
    else
        return try_emplace(value.first, forward<P>(value).second);
}

template<typename T>
template<typename P>
auto sorted_string_map<T>::insert(const_iterator hint, P&& value) -> iterator
    requires requires(P& p) { p.first(); p.second(); } || requires(P& p) { p.first; p.second; }
{
    if constexpr (requires(P& p) { p.first(); })
        return try_emplace(hint, value.first(), forward<P>(value).second());
    else
        return try_emplace(hint, value.first, forward<P>(value).second);
}

template<typename T>
template<typename I>
void sorted_string_map<T>::insert(I first, I last)
{
    for (; first != last; ++first)
        insert(*first);
}

template<typename T>
template<typename R>
void sorted_string_map<T>::insert_range(R&& r)
{
    for (auto&& value : r)
        insert(forward<decltype(value)>(value));
}

template<typename T>
template<typename M>
auto sorted_string_map<T>::insert_or_assign(StringView key, M&& obj) -> Pair<iterator, bool>
{
    const u32 index = _strings.find_lower_bound(key);
    const bool inserted = index == _strings.array_size || _strings.at(index) != key;
    if (inserted)
    {
        T value(forward<M>(obj));
        insert_at(index, key, move(value));
    }
    else
        _values[index] = forward<M>(obj);
    return {iterator{this, index}, inserted};
}

template<typename T>
template<typename M>
auto sorted_string_map<T>::insert_or_assign(const_iterator hint, StringView key, M&& obj) -> iterator
{
    const u32 index = _strings.find_lower_bound(key, hint._index);
    if (index == _strings.array_size || _strings.at(index) != key)
    {
        T value(forward<M>(obj));
        insert_at(index, key, move(value));
    }
    else
        _values[index] = forward<M>(obj);
    return iterator{this, index};
}

template<typename T>
template<typename... Args>
auto sorted_string_map<T>::emplace(StringView key, Args&&... args) -> Pair<iterator, bool>
{
    return try_emplace(key, forward<Args>(args)...);
}

template<typename T>
template<typename... Args>
auto sorted_string_map<T>::emplace_hint(const_iterator hint, StringView key, Args&&... args) -> iterator
{
    return try_emplace(hint, key, forward<Args>(args)...);
}

template<typename T>
template<typename... Args>
auto sorted_string_map<T>::try_emplace(StringView key, Args&&... args) -> Pair<iterator, bool>
{
    const u32 index = _strings.find_lower_bound(key);
    const bool inserted = index == _strings.array_size || _strings.at(index) != key;
    if (inserted)
    {
        T value(forward<Args>(args)...);
        insert_at(index, key, move(value));
    }
    return {iterator{this, index}, inserted};
}

template<typename T>
template<typename... Args>
auto sorted_string_map<T>::try_emplace(const_iterator hint, StringView key, Args&&... args) -> iterator
{
    const u32 index = _strings.find_lower_bound(key, hint._index);
    if (index == _strings.array_size || _strings.at(index) != key)
    {
        T value(forward<Args>(args)...);
        insert_at(index, key, move(value));
    }
    return iterator{this, index};
}

template<typename I>
    requires std::same_as<I, typename I::container_type::iterator> ||
             std::same_as<I, typename I::container_type::const_iterator>
I operator+(isize n, const I& it) noexcept
{
    return it + n;
}

template<typename T> void swap(sorted_string_map<T>& a, sorted_string_map<T>& b) noexcept { a.swap(b); }

template<typename T, typename Pred>
u32 erase_if(sorted_string_map<T>& m, Pred&& pred)
{
    auto& strings = m._strings;
    const u32 size = strings.array_size;
    u32 num_elements_kept = 0, num_string_bytes_removed = 0;
    for (u32 i = 0; i < size; i++)
    {
        const u32 offset = strings.offsets_array[i], key_size = strings.lengths_array[i];
        if (pred(m.begin()[i]))
        {
            destroy_at(m._values + i);
            num_string_bytes_removed += key_size + 1;
        }
        else
        {
            if (num_string_bytes_removed > 0)
            {
                char* key = strings.string_buffer + offset;
                sorted_string_map<T>::relocate(m._values + num_elements_kept, m._values + i, 1);
                std::memmove(key - num_string_bytes_removed, key, key_size + 1);
                strings.offsets_array[num_elements_kept] = offset - num_string_bytes_removed;
                strings.lengths_array[num_elements_kept] = key_size;
            }
            num_elements_kept++;
        }
    }
    strings.string_buffer_length -= num_string_bytes_removed;
    strings.array_size = num_elements_kept;
    return size - num_elements_kept;
}

} // namespace floormat
