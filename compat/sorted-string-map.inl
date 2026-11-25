#pragma once
#include "sorted-string-map.hpp"
#include "assert.hpp"
#include "construct-at.hpp"
#include "reverse-iterator.hpp"
#include <cstring>
#include <new>

namespace floormat {

template<typename T>
sorted_string_map<T>::const_iterator::const_iterator(const sorted_string_map* map, u32 index) noexcept:
    _map{map}, _index{index}
{
}

template<typename T> sorted_string_map<T>::const_iterator::const_iterator() noexcept = default;

template<typename T>
auto sorted_string_map<T>::const_iterator::pointer::operator->() const noexcept -> const reference*
{
    return &ref;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator*() const noexcept -> reference
{
    return {_map->_strings.at(_index), _map->_values[_index]};
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator->() const noexcept -> pointer
{
    return pointer{**this};
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator[](difference_type n) const noexcept -> reference
{
    return *(*this + n);
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator++() noexcept -> const_iterator&
{
    ++_index;
    return *this;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator++(int) noexcept -> const_iterator
{
    const_iterator previous = *this;
    ++_index;
    return previous;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator--() noexcept -> const_iterator&
{
    --_index;
    return *this;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator--(int) noexcept -> const_iterator
{
    const_iterator previous = *this;
    --_index;
    return previous;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator+=(difference_type n) noexcept -> const_iterator&
{
    _index = (u32)((isize)_index + n);
    return *this;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator-=(difference_type n) noexcept -> const_iterator&
{
    _index = (u32)((isize)_index - n);
    return *this;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator+(difference_type n) const noexcept -> const_iterator
{
    const_iterator ret = *this;
    ret += n;
    return ret;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator-(difference_type n) const noexcept -> const_iterator
{
    const_iterator ret = *this;
    ret -= n;
    return ret;
}

template<typename T>
auto sorted_string_map<T>::const_iterator::operator-(const const_iterator& other) const noexcept -> difference_type
{
    fm_debug3_assert(_map == other._map);
    return (isize)_index - (isize)other._index;
}

template<typename T>
bool sorted_string_map<T>::const_iterator::operator==(const const_iterator& other) const noexcept
{
    fm_debug3_assert(_map == other._map);
    return _index == other._index;
}

template<typename T>
std::strong_ordering sorted_string_map<T>::const_iterator::operator<=>(const const_iterator& other) const noexcept
{
    fm_debug3_assert(_map == other._map);
    return _index <=> other._index;
}

template<typename T>
sorted_string_map<T>::iterator::iterator(sorted_string_map* map, u32 index) noexcept: const_iterator{map, index}
{
}

template<typename T> sorted_string_map<T>::iterator::iterator() noexcept = default;

template<typename T>
auto sorted_string_map<T>::iterator::pointer::operator->() const noexcept -> const reference*
{
    return &ref;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator*() const noexcept -> reference
{
    return {this->_map->_strings.at(this->_index), this->_map->_values[this->_index]};
}

template<typename T>
auto sorted_string_map<T>::iterator::operator->() const noexcept -> pointer
{
    return pointer{**this};
}

template<typename T>
auto sorted_string_map<T>::iterator::operator[](difference_type n) const noexcept -> reference
{
    return *(*this + n);
}

template<typename T>
auto sorted_string_map<T>::iterator::operator++() noexcept -> iterator&
{
    ++this->_index;
    return *this;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator++(int) noexcept -> iterator
{
    iterator previous = *this;
    ++this->_index;
    return previous;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator--() noexcept -> iterator&
{
    --this->_index;
    return *this;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator--(int) noexcept -> iterator
{
    iterator previous = *this;
    --this->_index;
    return previous;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator+=(difference_type n) noexcept -> iterator&
{
    this->_index = (u32)((isize)this->_index + n);
    return *this;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator-=(difference_type n) noexcept -> iterator&
{
    this->_index = (u32)((isize)this->_index - n);
    return *this;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator+(difference_type n) const noexcept -> iterator
{
    iterator ret = *this;
    ret += n;
    return ret;
}

template<typename T>
auto sorted_string_map<T>::iterator::operator-(difference_type n) const noexcept -> iterator
{
    iterator ret = *this;
    ret -= n;
    return ret;
}

template<typename T> auto sorted_string_map<T>::begin() noexcept -> iterator { return iterator{this, 0}; }

template<typename T>
auto sorted_string_map<T>::begin() const noexcept -> const_iterator
{
    return const_iterator{this, 0};
}

template<typename T> auto sorted_string_map<T>::cbegin() const noexcept -> const_iterator { return begin(); }

template<typename T>
auto sorted_string_map<T>::end() noexcept -> iterator
{
    return iterator{this, _strings.array_size};
}

template<typename T>
auto sorted_string_map<T>::end() const noexcept -> const_iterator
{
    return const_iterator{this, _strings.array_size};
}

template<typename T> auto sorted_string_map<T>::cend() const noexcept -> const_iterator { return end(); }

template<typename T>
auto sorted_string_map<T>::rbegin() noexcept -> reverse_iterator
{
    return reverse_iterator{end()};
}

template<typename T>
auto sorted_string_map<T>::rbegin() const noexcept -> const_reverse_iterator
{
    return const_reverse_iterator{end()};
}

template<typename T> auto sorted_string_map<T>::crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }

template<typename T>
auto sorted_string_map<T>::rend() noexcept -> reverse_iterator
{
    return reverse_iterator{begin()};
}

template<typename T>
auto sorted_string_map<T>::rend() const noexcept -> const_reverse_iterator
{
    return const_reverse_iterator{begin()};
}

template<typename T> auto sorted_string_map<T>::crend() const noexcept -> const_reverse_iterator { return rend(); }

template<typename T> bool sorted_string_map<T>::empty() const noexcept { return _strings.array_size == 0; }
template<typename T> u32 sorted_string_map<T>::size() const noexcept { return _strings.array_size; }
template<typename T> u32 sorted_string_map<T>::max_size() const noexcept { return (u32)-1; }

template<typename T>
T* sorted_string_map<T>::allocate(u32 count)
{
    if (count == 0)
        return nullptr;
    else if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        return static_cast<T*>(::operator new(count * sizeof(T), std::align_val_t{alignof(T)}));
    else
        return static_cast<T*>(::operator new(count * sizeof(T)));
}

template<typename T>
void sorted_string_map<T>::deallocate(T* values)
{
    if constexpr (alignof(T) > __STDCPP_DEFAULT_NEW_ALIGNMENT__)
        ::operator delete(values, std::align_val_t{alignof(T)});
    else
        ::operator delete(values);
}

template<typename T>
void sorted_string_map<T>::relocate(T* dst, T* src, u32 count)
{
    if constexpr (std::is_trivially_copyable_v<T>)
    {
        if (count > 0)
            std::memmove(dst, src, count * sizeof(T));
    }
    else if (dst < src)
    {
        for (u32 i = 0; i < count; i++)
        {
            construct_at(dst + i, move(src[i]));
            destroy_at(src + i);
        }
    }
    else
    {
        for (u32 i = count; i > 0; i--)
        {
            construct_at(dst + i - 1, move(src[i - 1]));
            destroy_at(src + i - 1);
        }
    }
}

template<typename T>
void sorted_string_map<T>::insert_at(u32 index, StringView key, T&& value)
{
    const u32 size = _strings.array_size, capacity = _strings.array_capacity;
    _strings.insert(index, key);
    if (_strings.array_capacity != capacity)
    {
        T* values = allocate(_strings.array_capacity);
        relocate(values, _values, index);
        relocate(values + index + 1, _values + index, size - index);
        deallocate(_values);
        _values = values;
    }
    else
        relocate(_values + index + 1, _values + index, size - index);
    construct_at(_values + index, move(value));
}

template<typename T>
sorted_string_map<T>::sorted_string_map() noexcept = default;

template<typename T>
sorted_string_map<T>::sorted_string_map(const sorted_string_map& other) requires std::is_copy_constructible_v<T>:
    _strings{other._strings}, _values{allocate(_strings.array_capacity)}
{
    if constexpr (std::is_trivially_copyable_v<T>)
    {
        if (_strings.array_size > 0)
            std::memcpy(_values, other._values, _strings.array_size * sizeof(T));
    }
    else
        for (u32 i = 0; i < _strings.array_size; i++)
            construct_at(_values + i, other._values[i]);
}

template<typename T>
sorted_string_map<T>::sorted_string_map(sorted_string_map&& other) noexcept:
    _strings{move(other._strings)}, _values{other._values}
{
    other._values = nullptr;
}

template<typename T>
sorted_string_map<T>::sorted_string_map(std::initializer_list<Pair<StringView, T>> list)
    requires std::is_copy_constructible_v<T>
{
    reserve((u32)list.size());
    insert(list);
}

template<typename T>
sorted_string_map<T>& sorted_string_map<T>::operator=(const sorted_string_map& other)
    requires std::is_copy_constructible_v<T>
{
    sorted_string_map copy(other);
    swap(copy);
    return *this;
}

template<typename T>
sorted_string_map<T>& sorted_string_map<T>::operator=(sorted_string_map&& other) noexcept
{
    sorted_string_map moved(move(other));
    swap(moved);
    return *this;
}

template<typename T>
sorted_string_map<T>& sorted_string_map<T>::operator=(std::initializer_list<Pair<StringView, T>> list)
    requires std::is_copy_constructible_v<T>
{
    sorted_string_map copy(list);
    swap(copy);
    return *this;
}

template<typename T>
sorted_string_map<T>::~sorted_string_map() noexcept
{
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_nothrow_destructible_v<T>);
    clear();
    deallocate(_values);
}

template<typename T>
T& sorted_string_map<T>::at(StringView key)
{
    const u32 index = _strings.find(key);
    fm_assert(index < _strings.array_size);
    return _values[index];
}

template<typename T>
const T& sorted_string_map<T>::at(StringView key) const
{
    const u32 index = _strings.find(key);
    fm_assert(index < _strings.array_size);
    return _values[index];
}

template<typename T>
T& sorted_string_map<T>::operator[](StringView key) requires std::is_default_constructible_v<T>
{
    const u32 index = _strings.find_lower_bound(key);
    if (index == _strings.array_size || _strings.at(index) != key)
        insert_at(index, key, T());
    return _values[index];
}

template<typename T>
void sorted_string_map<T>::reserve(u32 count)
{
    if (count > _strings.array_capacity)
    {
        T* values = allocate(count);
        relocate(values, _values, _strings.array_size);
        deallocate(_values);
        _values = values;
        _strings.reserve(count);
    }
}

template<typename T>
void sorted_string_map<T>::clear() noexcept
{
    if constexpr (!std::is_trivially_destructible_v<T>)
        for (u32 i = 0; i < _strings.array_size; i++)
            destroy_at(_values + i);
    _strings.string_buffer_length = 0;
    _strings.array_size = 0;
}

template<typename T>
auto sorted_string_map<T>::insert(const Pair<StringView, T>& value) -> Pair<iterator, bool>
    requires std::is_copy_constructible_v<T>
{
    return try_emplace(value.first(), value.second());
}

template<typename T>
auto sorted_string_map<T>::insert(Pair<StringView, T>&& value) -> Pair<iterator, bool>
{
    const u32 index = _strings.find_lower_bound(value.first());
    const bool inserted = index == _strings.array_size || _strings.at(index) != value.first();
    if (inserted)
        insert_at(index, value.first(), move(value.second()));
    return {iterator{this, index}, inserted};
}

template<typename T>
auto sorted_string_map<T>::insert(const_iterator hint, const Pair<StringView, T>& value) -> iterator
    requires std::is_copy_constructible_v<T>
{
    return try_emplace(hint, value.first(), value.second());
}

template<typename T>
auto sorted_string_map<T>::insert(const_iterator hint, Pair<StringView, T>&& value) -> iterator
{
    const u32 index = _strings.find_lower_bound(value.first(), hint._index);
    if (index == _strings.array_size || _strings.at(index) != value.first())
        insert_at(index, value.first(), move(value.second()));
    return iterator{this, index};
}

template<typename T>
void sorted_string_map<T>::insert(std::initializer_list<Pair<StringView, T>> list)
    requires std::is_copy_constructible_v<T>
{
    for (const auto& pair : list)
        insert(pair);
}

template<typename T>
auto sorted_string_map<T>::erase(iterator pos) -> iterator
{
    return erase(const_iterator{pos});
}

template<typename T>
auto sorted_string_map<T>::erase(const_iterator pos) -> iterator
{
    return erase(pos, pos + 1);
}

template<typename T>
auto sorted_string_map<T>::erase(const_iterator first, const_iterator last) -> iterator
{
    fm_debug3_assert(first._map == this && last._map == this);
    fm_debug3_assert(first._index <= last._index && last._index <= _strings.array_size);
    if (first._index < last._index)
    {
        for (u32 i = first._index; i < last._index; i++)
            destroy_at(_values + i);
        relocate(_values + first._index, _values + last._index, _strings.array_size - last._index);
        _strings.remove(first._index, last._index);
    }
    return iterator{this, first._index};
}

template<typename T>
u32 sorted_string_map<T>::erase(StringView key)
{
    const u32 index = _strings.find(key);
    if (index < _strings.array_size)
    {
        erase(const_iterator{this, index});
        return 1;
    }
    else
        return 0;
}

template<typename T>
void sorted_string_map<T>::swap(sorted_string_map& other) noexcept
{
    _strings.swap(other._strings);
    floormat::swap(_values, other._values);
}

template<typename T> u32 sorted_string_map<T>::count(StringView key) const { return contains(key) ? 1 : 0; }

template<typename T>
auto sorted_string_map<T>::find(StringView key) -> iterator
{
    return iterator{this, _strings.find(key)};
}

template<typename T>
auto sorted_string_map<T>::find(StringView key) const -> const_iterator
{
    return const_iterator{this, _strings.find(key)};
}

template<typename T> bool sorted_string_map<T>::contains(StringView key) const
{
    return _strings.find(key) < _strings.array_size;
}

template<typename T>
auto sorted_string_map<T>::lower_bound(StringView key) -> iterator
{
    return iterator{this, _strings.find_lower_bound(key)};
}

template<typename T>
auto sorted_string_map<T>::lower_bound(StringView key) const -> const_iterator
{
    return const_iterator{this, _strings.find_lower_bound(key)};
}

template<typename T>
auto sorted_string_map<T>::upper_bound(StringView key) -> iterator
{
    return equal_range(key).second();
}

template<typename T>
auto sorted_string_map<T>::upper_bound(StringView key) const -> const_iterator
{
    return equal_range(key).second();
}

template<typename T>
auto sorted_string_map<T>::equal_range(StringView key) -> Pair<iterator, iterator>
{
    const u32 index = _strings.find_lower_bound(key);
    const u32 count = index < _strings.array_size && _strings.at(index) == key ? 1 : 0;
    return {iterator{this, index}, iterator{this, index + count}};
}

template<typename T>
auto sorted_string_map<T>::equal_range(StringView key) const -> Pair<const_iterator, const_iterator>
{
    const u32 index = _strings.find_lower_bound(key);
    const u32 count = index < _strings.array_size && _strings.at(index) == key ? 1 : 0;
    return {const_iterator{this, index}, const_iterator{this, index + count}};
}

template<typename T>
bool sorted_string_map<T>::operator==(const sorted_string_map& other) const
    requires requires(const T& x) { { x == x } -> std::convertible_to<bool>; }
{
    bool is_equal = size() == other.size();
    for (auto ia = begin(), ib = other.begin(); is_equal && ia != end(); ++ia, ++ib)
        is_equal = ia->first == ib->first && bool(ia->second == ib->second);
    return is_equal;
}

} // namespace floormat
