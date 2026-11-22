#pragma once
#include "assert.hpp"
#include "container-traits.hpp"
#include "move.hpp"
#include "range-access.hpp"
#include "fwd/construct_at.hpp"
#include <cr/ArrayView.h>
#include <cr/Macros.h>
#include <compare>
#include <concepts>
#include <initializer_list>
#include <type_traits>
#include <utility>

namespace floormat::detail::inplace_array_impl {

template<typename S, typename I>
concept sentinel_for = requires(I& it, const S& s) { *it; ++it; it != s; };

template<typename T>
using synth_three_way_result = std::conditional_t<std::three_way_comparable<T>, std::compare_three_way_result<T>,
                                                  std::type_identity<std::weak_ordering>>::type;

template<typename T>
concept less_than_comparable = requires(const T& a) { { a < a } -> std::convertible_to<bool>; };

template<typename T, usize N>
struct element_array
{
    T data[N];

    constexpr explicit element_array(ValueInitT);
    constexpr element_array(DirectInitT, const T& value);
    template<usize... Is> constexpr element_array(const T& value, std::index_sequence<Is...>);
};

template<typename T, usize N>
union storage
{
    // GCC 16.2 rejects a constant expression that leaves a union with no active member.
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=110245
    char null_state;
    element_array<T, N> array;

    constexpr storage() noexcept;
    storage(const storage&) = default;
    storage(storage&&) = default;
    storage& operator=(const storage&) = default;
    storage& operator=(storage&&) = default;
    ~storage() requires std::is_trivially_destructible_v<T> = default;
    constexpr ~storage();
};

} // namespace floormat::detail::inplace_array_impl

namespace floormat {

template<typename I> class reverse_iterator;

template<typename T, usize N>
class inplace_array
{
    static_assert(N <= 0xffffffff);

    using stored_size_type = std::conditional_t<N <= 0xff && alignof(T) <= 1, u8,
                                                std::conditional_t<N <= 0xffff && alignof(T) <= 2, u16, u32>>;

    detail::inplace_array_impl::storage<T, N> _storage;
    stored_size_type _size = 0;

    constexpr void start_storage_lifetime();
    constexpr void start_storage_lifetime(const T& value);

public:
    using value_type = T;
    using size_type = u32;
    using difference_type = isize;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = floormat::reverse_iterator<T*>;
    using const_reverse_iterator = floormat::reverse_iterator<const T*>;

    inplace_array() noexcept = default;
    constexpr explicit inplace_array(u32 count);
    constexpr inplace_array(u32 count, const T& value);
    template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
    constexpr inplace_array(I first, S last);
    constexpr inplace_array(std::initializer_list<T> list);

    inplace_array(const inplace_array&) requires std::is_trivially_copy_constructible_v<T> = default;
    constexpr inplace_array(const inplace_array& other)
        requires (!std::is_trivially_copy_constructible_v<T> && std::is_copy_constructible_v<T>);
    inplace_array(inplace_array&&) requires std::is_trivially_move_constructible_v<T> = default;
    constexpr inplace_array(inplace_array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>);

    inplace_array& operator=(const inplace_array&)
        requires (std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> &&
                  std::is_trivially_destructible_v<T>) = default;
    constexpr inplace_array& operator=(const inplace_array& other)
        requires (!(std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> &&
                    std::is_trivially_destructible_v<T>) && std::is_copy_constructible_v<T>);
    inplace_array& operator=(inplace_array&&)
        requires (std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> &&
                  std::is_trivially_destructible_v<T>) = default;
    constexpr inplace_array& operator=(inplace_array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires (!(std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> &&
                    std::is_trivially_destructible_v<T>) && std::is_move_constructible_v<T>);
    constexpr inplace_array& operator=(std::initializer_list<T> list);

    ~inplace_array() requires std::is_trivially_destructible_v<T> = default;
    constexpr ~inplace_array();

    constexpr void assign(u32 count, const T& value);
    template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
    constexpr void assign(I first, S last);
    constexpr void assign(std::initializer_list<T> list);
    template<typename R> requires range<R> constexpr void assign_range(R&& r);

    constexpr T& at(u32 i);
    constexpr const T& at(u32 i) const;
    constexpr CORRADE_ALWAYS_INLINE T& operator[](u32 i) noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T& operator[](u32 i) const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T& front() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T& front() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T& back() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T& back() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T* data() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* data() const noexcept;

    constexpr CORRADE_ALWAYS_INLINE T* begin() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* begin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T* end() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* end() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* cbegin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* cend() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator rbegin() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator rbegin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator rend() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator rend() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator crbegin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator crend() const noexcept;

    constexpr CORRADE_ALWAYS_INLINE bool empty() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE u32 size() const noexcept;
    static constexpr CORRADE_ALWAYS_INLINE u32 max_size() noexcept;
    static constexpr CORRADE_ALWAYS_INLINE u32 capacity() noexcept;
    constexpr void resize(u32 count);
    constexpr void resize(u32 count, const T& value);

    template<typename... Args> constexpr T& emplace_back(Args&&... args);
    constexpr T& push_back(const T& value);
    constexpr T& push_back(T&& value);
    template<typename... Args> constexpr T* try_emplace_back(Args&&... args);
    constexpr T* try_push_back(const T& value);
    constexpr T* try_push_back(T&& value);
    template<typename... Args> constexpr T& unchecked_emplace_back(Args&&... args);
    constexpr T& unchecked_push_back(const T& value);
    constexpr T& unchecked_push_back(T&& value);
    constexpr void pop_back() noexcept;
    template<typename R> requires range<R> constexpr void append_range(R&& r);

    template<typename... Args> constexpr T* emplace(const T* pos, Args&&... args);
    constexpr T* insert(const T* pos, const T& value);
    constexpr T* insert(const T* pos, T&& value);
    constexpr T* insert(const T* pos, u32 count, const T& value);
    template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
    constexpr T* insert(const T* pos, I first, S last);
    constexpr T* insert(const T* pos, std::initializer_list<T> list);
    template<typename R> requires range<R> constexpr T* insert_range(const T* pos, R&& r);
    constexpr T* erase(const T* pos);
    constexpr T* erase(const T* first, const T* last);
    constexpr void clear() noexcept;
    constexpr void swap(inplace_array& other)
        noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_assignable_v<T>);

    constexpr CORRADE_ALWAYS_INLINE operator ArrayView<T>() noexcept;
    constexpr CORRADE_ALWAYS_INLINE operator ArrayView<const T>() const noexcept;
};

template<typename T>
class inplace_array<T, 0>
{
public:
    using value_type = T;
    using size_type = u32;
    using difference_type = isize;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = floormat::reverse_iterator<T*>;
    using const_reverse_iterator = floormat::reverse_iterator<const T*>;

    inplace_array() noexcept = default;
    constexpr explicit inplace_array(u32 count);
    constexpr inplace_array(u32 count, const T& value);
    template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
    constexpr inplace_array(I first, S last);
    constexpr inplace_array(std::initializer_list<T> list);
    constexpr inplace_array& operator=(std::initializer_list<T> list);

    constexpr void assign(u32 count, const T& value);
    template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
    constexpr void assign(I first, S last);
    constexpr void assign(std::initializer_list<T> list);
    template<typename R> requires range<R> constexpr void assign_range(R&& r);

    constexpr T& at(u32 i);
    constexpr const T& at(u32 i) const;
    constexpr CORRADE_ALWAYS_INLINE T& operator[](u32 i) noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T& operator[](u32 i) const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T& front() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T& front() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T& back() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T& back() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T* data() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* data() const noexcept;

    constexpr CORRADE_ALWAYS_INLINE T* begin() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* begin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE T* end() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* end() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* cbegin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const T* cend() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator rbegin() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator rbegin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE reverse_iterator rend() noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator rend() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator crbegin() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE const_reverse_iterator crend() const noexcept;

    constexpr CORRADE_ALWAYS_INLINE bool empty() const noexcept;
    constexpr CORRADE_ALWAYS_INLINE u32 size() const noexcept;
    static constexpr CORRADE_ALWAYS_INLINE u32 max_size() noexcept;
    static constexpr CORRADE_ALWAYS_INLINE u32 capacity() noexcept;
    constexpr void resize(u32 count);
    constexpr void resize(u32 count, const T& value);

    template<typename... Args> constexpr T& emplace_back(Args&&... args);
    constexpr T& push_back(const T& value);
    constexpr T& push_back(T&& value);
    template<typename... Args> constexpr T* try_emplace_back(Args&&... args);
    constexpr T* try_push_back(const T& value);
    constexpr T* try_push_back(T&& value);
    template<typename... Args> constexpr T& unchecked_emplace_back(Args&&... args);
    constexpr T& unchecked_push_back(const T& value);
    constexpr T& unchecked_push_back(T&& value);
    constexpr void pop_back() noexcept;
    template<typename R> requires range<R> constexpr void append_range(R&& r);

    template<typename... Args> constexpr T* emplace(const T* pos, Args&&... args);
    constexpr T* insert(const T* pos, const T& value);
    constexpr T* insert(const T* pos, T&& value);
    constexpr T* insert(const T* pos, u32 count, const T& value);
    template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
    constexpr T* insert(const T* pos, I first, S last);
    constexpr T* insert(const T* pos, std::initializer_list<T> list);
    template<typename R> requires range<R> constexpr T* insert_range(const T* pos, R&& r);
    constexpr T* erase(const T* pos);
    constexpr T* erase(const T* first, const T* last);
    constexpr void clear() noexcept;
    constexpr void swap(inplace_array& other) noexcept;

    constexpr CORRADE_ALWAYS_INLINE operator ArrayView<T>() noexcept;
    constexpr CORRADE_ALWAYS_INLINE operator ArrayView<const T>() const noexcept;
};

template<typename T, usize N>
constexpr bool operator==(const inplace_array<T, N>& a, const inplace_array<T, N>& b);
template<typename T, usize N> requires detail::inplace_array_impl::less_than_comparable<T>
constexpr detail::inplace_array_impl::synth_three_way_result<T>
operator<=>(const inplace_array<T, N>& a, const inplace_array<T, N>& b);
template<typename T, usize N>
constexpr void swap(inplace_array<T, N>& a, inplace_array<T, N>& b) noexcept(noexcept(a.swap(b)));
template<typename T, usize N, typename U = T>
constexpr u32 erase(inplace_array<T, N>& c, const U& value);

template<typename T, usize N>
struct container_traits<inplace_array<T, N>>
{
    static constexpr void remove_suffix(inplace_array<T, N>& c, usize count);
};

} // namespace floormat

namespace floormat::detail::inplace_array_impl {

template<typename T, usize N>
constexpr element_array<T, N>::element_array(ValueInitT): data() {}

template<typename T, usize N>
constexpr element_array<T, N>::element_array(DirectInitT, const T& value):
    element_array{value, std::make_index_sequence<N>{}}
{}

template<typename T, usize N>
template<usize... Is>
constexpr element_array<T, N>::element_array(const T& value, std::index_sequence<Is...>): data{((void)Is, value)...} {}

template<typename T, usize N>
constexpr storage<T, N>::storage() noexcept
{
    if consteval
    {
        null_state = 0;
    }
}

template<typename T, usize N>
constexpr storage<T, N>::~storage() {}

} // namespace floormat::detail::inplace_array_impl

namespace floormat {

template<typename T, usize N>
constexpr void inplace_array<T, N>::start_storage_lifetime()
{
    if consteval
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            std::construct_at(&_storage.array, ValueInit);
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (u32 i = 0; i < N; i++)
                    std::destroy_at(_storage.array.data + i);
        }
    }
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::start_storage_lifetime(const T& value)
{
    if consteval
    {
        std::construct_at(&_storage.array, DirectInit, value);
        if constexpr (!std::is_trivially_destructible_v<T>)
            for (u32 i = 0; i < N; i++)
                std::destroy_at(_storage.array.data + i);
    }
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(u32 count): inplace_array{}
{
    resize(count);
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
constexpr inplace_array<T, N>::inplace_array(const inplace_array& other)
    requires (!std::is_trivially_copy_constructible_v<T> && std::is_copy_constructible_v<T>):
    inplace_array{}
{
    for (const T& x : other)
        unchecked_emplace_back(x);
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(inplace_array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires (!std::is_trivially_move_constructible_v<T> && std::is_move_constructible_v<T>):
    inplace_array{}
{
    for (T& x : other)
        unchecked_emplace_back(move(x));
}

template<typename T, usize N>
constexpr inplace_array<T, N>& inplace_array<T, N>::operator=(const inplace_array& other)
    requires (!(std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> &&
                std::is_trivially_destructible_v<T>) && std::is_copy_constructible_v<T>)
{
    if (this != &other)
    {
        clear();
        for (const T& x : other)
            unchecked_emplace_back(x);
    }
    return *this;
}

template<typename T, usize N>
constexpr inplace_array<T, N>& inplace_array<T, N>::operator=(inplace_array&& other)
    noexcept(std::is_nothrow_move_constructible_v<T>)
    requires (!(std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> &&
                std::is_trivially_destructible_v<T>) && std::is_move_constructible_v<T>)
{
    if (this != &other)
    {
        clear();
        for (T& x : other)
            unchecked_emplace_back(move(x));
    }
    return *this;
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
constexpr inplace_array<T, N>::~inplace_array()
{
    clear();
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

template<typename T, usize N> constexpr T* inplace_array<T, N>::data() noexcept { return _storage.array.data; }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::data() const noexcept { return _storage.array.data; }
template<typename T, usize N> constexpr T* inplace_array<T, N>::begin() noexcept { return data(); }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::begin() const noexcept { return data(); }
template<typename T, usize N> constexpr T* inplace_array<T, N>::end() noexcept { return data() + size(); }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::end() const noexcept { return data() + size(); }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::cbegin() const noexcept { return data(); }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::cend() const noexcept { return data() + size(); }
template<typename T, usize N> constexpr bool inplace_array<T, N>::empty() const noexcept { return _size == 0; }
template<typename T, usize N> constexpr u32 inplace_array<T, N>::size() const noexcept { return _size; }
template<typename T, usize N> constexpr u32 inplace_array<T, N>::max_size() noexcept { return (u32)N; }
template<typename T, usize N> constexpr u32 inplace_array<T, N>::capacity() noexcept { return (u32)N; }

template<typename T, usize N>
constexpr void inplace_array<T, N>::resize(u32 count)
{
    fm_assert(count <= N);
    if (count < size())
        erase(begin() + count, end());
    else
        while (size() < count)
            unchecked_emplace_back();
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
template<typename... Args>
constexpr T* inplace_array<T, N>::try_emplace_back(Args&&... args)
{
    if (size() < capacity())
    {
        unchecked_emplace_back(forward<Args>(args)...);
        return end() - 1;
    }
    else
        return nullptr;
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::try_push_back(const T& value)
{
    return try_emplace_back(value);
}

template<typename T, usize N>
constexpr T* inplace_array<T, N>::try_push_back(T&& value)
{
    return try_emplace_back(move(value));
}

template<typename T, usize N>
template<typename... Args>
constexpr T& inplace_array<T, N>::unchecked_emplace_back(Args&&... args)
{
    fm_debug3_assert(size() < capacity());
    if constexpr (!std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>)
    {
        if (std::is_constant_evaluated() && empty())
        {
            T value(forward<Args>(args)...);
            start_storage_lifetime(value);
            std::construct_at(data(), move(value));
        }
        else
            std::construct_at(data() + size(), forward<Args>(args)...);
    }
    else
    {
        if (std::is_constant_evaluated() && empty())
            start_storage_lifetime();
        std::construct_at(data() + size(), forward<Args>(args)...);
    }
    return data()[_size++];
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
        std::destroy_at(data() + size());
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
    T* const from = begin() + (first - cbegin());
    T* out = from;
    for (T* p = begin() + (last - cbegin()); p != end(); ++p, ++out)
        *out = move(*p);
    if constexpr (!std::is_trivially_destructible_v<T>)
        for (T* p = out; p != end(); ++p)
            std::destroy_at(p);
    _size = (stored_size_type)(out - begin());
    return from;
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::clear() noexcept
{
    if constexpr (!std::is_trivially_destructible_v<T>)
        for (T* p = begin(); p != end(); ++p)
            std::destroy_at(p);
    _size = 0;
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

template<typename T>
constexpr inplace_array<T, 0>::inplace_array(u32 count)
{
    fm_assert(count == 0);
}

template<typename T>
constexpr inplace_array<T, 0>::inplace_array(u32 count, const T&)
{
    fm_assert(count == 0);
}

template<typename T>
template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
constexpr inplace_array<T, 0>::inplace_array(I first, S last)
{
    fm_assert(!(first != last));
}

template<typename T>
constexpr inplace_array<T, 0>::inplace_array(std::initializer_list<T> list)
{
    fm_assert(list.size() == 0);
}

template<typename T>
constexpr inplace_array<T, 0>& inplace_array<T, 0>::operator=(std::initializer_list<T> list)
{
    fm_assert(list.size() == 0);
    return *this;
}

template<typename T>
constexpr T& inplace_array<T, 0>::operator[](u32 i) noexcept
{
    fm_debug_assert(i < size());
    return data()[i];
}

template<typename T>
constexpr const T& inplace_array<T, 0>::operator[](u32 i) const noexcept
{
    fm_debug_assert(i < size());
    return data()[i];
}

template<typename T>
constexpr T& inplace_array<T, 0>::front() noexcept
{
    fm_debug_assert(!empty());
    return *data();
}

template<typename T>
constexpr const T& inplace_array<T, 0>::front() const noexcept
{
    fm_debug_assert(!empty());
    return *data();
}

template<typename T>
constexpr T& inplace_array<T, 0>::back() noexcept
{
    fm_debug_assert(!empty());
    return *data();
}

template<typename T>
constexpr const T& inplace_array<T, 0>::back() const noexcept
{
    fm_debug_assert(!empty());
    return *data();
}

template<typename T> constexpr T* inplace_array<T, 0>::data() noexcept { return nullptr; }
template<typename T> constexpr const T* inplace_array<T, 0>::data() const noexcept { return nullptr; }
template<typename T> constexpr T* inplace_array<T, 0>::begin() noexcept { return nullptr; }
template<typename T> constexpr const T* inplace_array<T, 0>::begin() const noexcept { return nullptr; }
template<typename T> constexpr T* inplace_array<T, 0>::end() noexcept { return nullptr; }
template<typename T> constexpr const T* inplace_array<T, 0>::end() const noexcept { return nullptr; }
template<typename T> constexpr const T* inplace_array<T, 0>::cbegin() const noexcept { return nullptr; }
template<typename T> constexpr const T* inplace_array<T, 0>::cend() const noexcept { return nullptr; }
template<typename T> constexpr bool inplace_array<T, 0>::empty() const noexcept { return true; }
template<typename T> constexpr u32 inplace_array<T, 0>::size() const noexcept { return 0; }
template<typename T> constexpr u32 inplace_array<T, 0>::max_size() noexcept { return 0; }
template<typename T> constexpr u32 inplace_array<T, 0>::capacity() noexcept { return 0; }

template<typename T>
constexpr void inplace_array<T, 0>::resize(u32 count)
{
    fm_assert(count == 0);
}

template<typename T>
constexpr void inplace_array<T, 0>::resize(u32 count, const T&)
{
    fm_assert(count == 0);
}

template<typename T>
template<typename... Args>
constexpr T& inplace_array<T, 0>::emplace_back(Args&&... args)
{
    fm_assert(size() < capacity());
    return unchecked_emplace_back(forward<Args>(args)...);
}

template<typename T>
constexpr T& inplace_array<T, 0>::push_back(const T& value)
{
    return emplace_back(value);
}

template<typename T>
constexpr T& inplace_array<T, 0>::push_back(T&& value)
{
    return emplace_back(move(value));
}

template<typename T>
template<typename... Args>
constexpr T* inplace_array<T, 0>::try_emplace_back(Args&&...)
{
    return nullptr;
}

template<typename T> constexpr T* inplace_array<T, 0>::try_push_back(const T&) { return nullptr; }
template<typename T> constexpr T* inplace_array<T, 0>::try_push_back(T&&) { return nullptr; }

template<typename T>
template<typename... Args>
constexpr T& inplace_array<T, 0>::unchecked_emplace_back(Args&&... args)
{
    fm_debug3_assert(size() < capacity());
    return *std::construct_at(data(), forward<Args>(args)...);
}

template<typename T>
constexpr T& inplace_array<T, 0>::unchecked_push_back(const T& value)
{
    return unchecked_emplace_back(value);
}

template<typename T>
constexpr T& inplace_array<T, 0>::unchecked_push_back(T&& value)
{
    return unchecked_emplace_back(move(value));
}

template<typename T>
constexpr void inplace_array<T, 0>::pop_back() noexcept
{
    fm_debug_assert(!empty());
}

template<typename T>
constexpr T* inplace_array<T, 0>::erase(const T* pos)
{
    fm_debug_assert(pos < cend());
    return data();
}

template<typename T>
constexpr T* inplace_array<T, 0>::erase(const T* first, const T* last)
{
    fm_debug_assert(first == last);
    return data();
}

template<typename T> constexpr void inplace_array<T, 0>::clear() noexcept {}
template<typename T> constexpr inplace_array<T, 0>::operator ArrayView<T>() noexcept { return {}; }
template<typename T> constexpr inplace_array<T, 0>::operator ArrayView<const T>() const noexcept { return {}; }

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
