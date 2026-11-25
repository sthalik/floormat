#include "sorted-string-map.hpp"
#include "assert.hpp"
#include "defs.hpp"
#include "lower-bound.hpp"
#include "move.hpp"
#include <cstring>
#include <cr/Math.h>

#if defined __GNUC__ && !defined __clang__
#pragma GCC diagnostic ignored "-Wattributes"
#endif

namespace floormat::detail::sorted_string_map {

namespace {

constexpr u32 min_array_capacity = 8, min_string_buffer_capacity = 64;

u32 new_capacity(u32 capacity, u32 needed, u32 minimum)
{
    const auto capacity_times_1_5 = (u32)Utility::min((u64)capacity + capacity/2, (u64)(u32)-1);
    return Utility::max(Utility::max(capacity_times_1_5, needed), minimum);
}

} // namespace

Strings::Strings() noexcept:
    string_buffer{}, offsets_array{}, lengths_array{},
    string_buffer_length{}, string_buffer_capacity{}, array_size{}, array_capacity{}
{
}

Strings::Strings(const Strings& other): Strings{}
{
    if (other.array_size > 0)
    {
        string_buffer = new char[other.string_buffer_length];
        offsets_array = new u32[other.array_size];
        lengths_array = new u32[other.array_size];
        std::memcpy(string_buffer, other.string_buffer, other.string_buffer_length);
        std::memcpy(offsets_array, other.offsets_array, other.array_size * sizeof(u32));
        std::memcpy(lengths_array, other.lengths_array, other.array_size * sizeof(u32));
        string_buffer_length = string_buffer_capacity = other.string_buffer_length;
        array_size = array_capacity = other.array_size;
    }
}

Strings::Strings(Strings&& other) noexcept: Strings{}
{
    swap(other);
}

Strings& Strings::operator=(const Strings& other)
{
    Strings copy{other};
    swap(copy);
    return *this;
}

Strings& Strings::operator=(Strings&& other) noexcept
{
    Strings moved{move(other)};
    swap(moved);
    return *this;
}

Strings::~Strings() noexcept
{
    delete[] string_buffer;
    delete[] offsets_array;
    delete[] lengths_array;
}

void Strings::swap(Strings& other) noexcept
{
    floormat::swap(string_buffer, other.string_buffer);
    floormat::swap(offsets_array, other.offsets_array);
    floormat::swap(lengths_array, other.lengths_array);
    floormat::swap(string_buffer_length, other.string_buffer_length);
    floormat::swap(string_buffer_capacity, other.string_buffer_capacity);
    floormat::swap(array_size, other.array_size);
    floormat::swap(array_capacity, other.array_capacity);
}

[[fm_always_inline]] StringView Strings::at(u32 index) const
{
    fm_debug3_assert(index < array_size);
    return StringView{string_buffer + offsets_array[index], lengths_array[index], StringViewFlag::NullTerminated};
}

void Strings::reserve(u32 reserve_size)
{
    if (reserve_size > array_capacity)
    {
        auto* offsets = new u32[reserve_size];
        auto* lengths = new u32[reserve_size];
        if (array_size > 0)
        {
            std::memcpy(offsets, offsets_array, array_size * sizeof(u32));
            std::memcpy(lengths, lengths_array, array_size * sizeof(u32));
        }
        delete[] offsets_array;
        delete[] lengths_array;
        offsets_array = offsets;
        lengths_array = lengths;
        array_capacity = reserve_size;
    }
}

u32 Strings::find_lower_bound(StringView s) const
{
    // lower_bound() passes references into offsets_array, so an element's address gives its index.
    auto is_less = [this](const u32& offset, StringView value) { return at((u32)(&offset - offsets_array)) < value; };
    return (u32)(lower_bound(offsets_array, offsets_array + array_size, s, is_less) - offsets_array);
}

u32 Strings::find_lower_bound(StringView s, u32 hint) const
{
    if (hint <= array_size && (hint == array_size || s <= at(hint)) && (hint == 0 || at(hint - 1) < s))
        return hint;
    else
        return find_lower_bound(s);
}

u32 Strings::find(StringView s) const
{
    const u32 index = find_lower_bound(s);
    if (index < array_size && at(index) == s)
        return index;
    else
        return array_size;
}

void Strings::insert(u32 index, StringView s)
{
    fm_assert(index <= array_size);
    fm_assert(array_size < (u32)-1 && s.size() < (usize)((u32)-1 - string_buffer_length));
    const auto size = (u32)s.size();
    const u32 offset = index < array_size ? offsets_array[index] : string_buffer_length;
    const u32 added = size + 1, length = string_buffer_length + added;

    if (length > string_buffer_capacity)
    {
        char* old_buffer = string_buffer;
        string_buffer_capacity = new_capacity(string_buffer_capacity, length, min_string_buffer_capacity);
        string_buffer = new char[string_buffer_capacity];
        if (string_buffer_length > 0)
        {
            std::memcpy(string_buffer, old_buffer, offset);
            std::memcpy(string_buffer + offset + added, old_buffer + offset, string_buffer_length - offset);
        }
        if (size > 0)
            std::memcpy(string_buffer + offset, s.data(), size);
        delete[] old_buffer;
    }
    else
    {
        // The shift leaves the added bytes at offset unchanged, so a key starting before offset is still at s.
        const uptr s_offset = (uptr)s.data() - (uptr)string_buffer;
        if (s_offset >= offset && s_offset < string_buffer_length)
            s = {s.data() + added, size};
        std::memmove(string_buffer + offset + added, string_buffer + offset, string_buffer_length - offset);
        if (size > 0)
            std::memmove(string_buffer + offset, s.data(), size);
    }
    string_buffer[offset + size] = '\0';
    string_buffer_length = length;

    if (array_size == array_capacity)
        reserve(new_capacity(array_capacity, array_size + 1, min_array_capacity));
    for (u32 i = array_size; i > index; i--)
    {
        offsets_array[i] = offsets_array[i - 1] + added;
        lengths_array[i] = lengths_array[i - 1];
    }
    offsets_array[index] = offset;
    lengths_array[index] = size;
    array_size++;
}

void Strings::remove(u32 first, u32 last)
{
    fm_assert(first < last && last <= array_size);
    const u32 offset = offsets_array[first];
    const u32 removed = offsets_array[last - 1] + lengths_array[last - 1] + 1 - offset;
    std::memmove(string_buffer + offset, string_buffer + offset + removed, string_buffer_length - offset - removed);
    string_buffer_length -= removed;
    for (u32 i = first, j = last; j < array_size; i++, j++)
    {
        offsets_array[i] = offsets_array[j] - removed;
        lengths_array[i] = lengths_array[j];
    }
    array_size -= last - first;
}

} // namespace floormat::detail::sorted_string_map
