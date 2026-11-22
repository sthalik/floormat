#pragma once
#include "assert.hpp"
#include "construct-at.hpp"
#include "container-traits.hpp"
#include "defs.hpp"
#include "move.hpp"
#include "range-access.hpp"
#include <cr/ArrayView.h>
#include <cr/Macros.h>
#include <compare>
#include <concepts>
#include <initializer_list>
#include <type_traits>
#include <utility>
#if fm_ASAN
#include <sanitizer/asan_interface.h>
#endif

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
    constexpr storage(const storage&) = default;
    constexpr storage(storage&&) = default;
    constexpr storage& operator=(const storage&) = default;
    constexpr storage& operator=(storage&&) = default;
    constexpr ~storage() noexcept requires std::is_trivially_destructible_v<T> = default;
    constexpr ~storage() noexcept;
};

} // namespace floormat::detail::inplace_array_impl

namespace floormat {

template<typename I> class reverse_iterator;

template<typename T, usize N>
class inplace_array
{
    static_assert(N > 0 && N <= 0xffffffff);

    using stored_size_type = std::conditional_t<N <= 0xff && alignof(T) <= 1, u8,
                                                std::conditional_t<N <= 0xffff && alignof(T) <= 2, u16, u32>>;

    detail::inplace_array_impl::storage<T, N> _storage;
    stored_size_type _size = 0;

    constexpr void start_storage_lifetime();
    constexpr void start_storage_lifetime(const T& value);
    constexpr void annotate_contiguous_container(u32 old_size, u32 new_size) noexcept;

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

    constexpr inplace_array() noexcept;
    constexpr explicit inplace_array(u32 count);
    constexpr inplace_array(NoInitT, u32 count);
    constexpr inplace_array(u32 count, const T& value);
    template<typename I, typename S> requires detail::inplace_array_impl::sentinel_for<S, I>
    constexpr inplace_array(I first, S last);
    constexpr inplace_array(std::initializer_list<T> list);

    inplace_array(const inplace_array&) requires (!fm_ASAN && std::is_trivially_copy_constructible_v<T>) = default;
    constexpr inplace_array(const inplace_array& other)
        requires ((!!fm_ASAN || !std::is_trivially_copy_constructible_v<T>) && std::is_copy_constructible_v<T>);
    inplace_array(inplace_array&&) requires (!fm_ASAN && std::is_trivially_move_constructible_v<T>) = default;
    constexpr inplace_array(inplace_array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires ((!!fm_ASAN || !std::is_trivially_move_constructible_v<T>) && std::is_move_constructible_v<T>);

    inplace_array& operator=(const inplace_array&)
        requires (!fm_ASAN && std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> &&
                  std::is_trivially_destructible_v<T>) = default;
    constexpr inplace_array& operator=(const inplace_array& other)
        requires ((!!fm_ASAN || !(std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> &&
                              std::is_trivially_destructible_v<T>)) && std::is_copy_constructible_v<T>);
    inplace_array& operator=(inplace_array&&)
        requires (!fm_ASAN && std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> &&
                  std::is_trivially_destructible_v<T>) = default;
    constexpr inplace_array& operator=(inplace_array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires ((!!fm_ASAN || !(std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> &&
                              std::is_trivially_destructible_v<T>)) && std::is_move_constructible_v<T>);
    constexpr inplace_array& operator=(std::initializer_list<T> list);

    ~inplace_array() noexcept requires (!fm_ASAN && std::is_trivially_destructible_v<T>) = default;
    constexpr ~inplace_array() noexcept;

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
    constexpr void resize(NoInitT, u32 count);
    constexpr void resize(u32 count, const T& value);

    template<typename... Args> constexpr T& emplace_back(Args&&... args);
    constexpr T& push_back(const T& value);
    constexpr T& push_back(T&& value);
    constexpr T& push_back(NoInitT);
    template<typename... Args> constexpr Optional<T&> try_emplace_back(Args&&... args);
    constexpr Optional<T&> try_push_back(const T& value);
    constexpr Optional<T&> try_push_back(T&& value);
    constexpr Optional<T&> try_push_back(NoInitT);
    template<typename... Args> constexpr T& unchecked_emplace_back(Args&&... args);
    constexpr T& unchecked_push_back(const T& value);
    constexpr T& unchecked_push_back(T&& value);
    constexpr T& unchecked_push_back(NoInitT);
    constexpr void pop_back() noexcept;
    template<typename R> requires range<R> constexpr void append_range(R&& r);

    template<typename... Args> constexpr T* emplace(const T* pos, Args&&... args);
    constexpr T* insert(const T* pos, const T& value);
    constexpr T* insert(const T* pos, T&& value);
    constexpr T* insert(const T* pos, NoInitT);
    constexpr T* insert(const T* pos, u32 count, const T& value);
    constexpr T* insert(const T* pos, NoInitT, u32 count);
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
constexpr storage<T, N>::~storage() noexcept {}

} // namespace floormat::detail::inplace_array_impl

namespace floormat {

template<typename T, usize N>
constexpr void inplace_array<T, N>::start_storage_lifetime()
{
    if consteval
    {
        if constexpr (std::is_default_constructible_v<T>)
        {
            construct_at(&_storage.array, ValueInit);
            if constexpr (!std::is_trivially_destructible_v<T>)
                for (u32 i = 0; i < N; i++)
                    destroy_at(_storage.array.data + i);
        }
    }
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::start_storage_lifetime(const T& value)
{
    if consteval
    {
        construct_at(&_storage.array, DirectInit, value);
        if constexpr (!std::is_trivially_destructible_v<T>)
            for (u32 i = 0; i < N; i++)
                destroy_at(_storage.array.data + i);
    }
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::annotate_contiguous_container([[maybe_unused]] u32 old_size,
                                                                  [[maybe_unused]] u32 new_size) noexcept
{
#if fm_ASAN
    if !consteval
    {
        __sanitizer_annotate_contiguous_container(data(), data() + N, data() + old_size, data() + new_size);
    }
#endif
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array() noexcept
{
    annotate_contiguous_container(capacity(), 0);
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(const inplace_array& other)
    requires ((!!fm_ASAN || !std::is_trivially_copy_constructible_v<T>) && std::is_copy_constructible_v<T>):
    inplace_array{}
{
    for (const T& x : other)
        unchecked_emplace_back(x);
}

template<typename T, usize N>
constexpr inplace_array<T, N>::inplace_array(inplace_array&& other) noexcept(std::is_nothrow_move_constructible_v<T>)
    requires ((!!fm_ASAN || !std::is_trivially_move_constructible_v<T>) && std::is_move_constructible_v<T>):
    inplace_array{}
{
    for (T& x : other)
        unchecked_emplace_back(move(x));
}

template<typename T, usize N>
constexpr inplace_array<T, N>& inplace_array<T, N>::operator=(const inplace_array& other)
    requires ((!!fm_ASAN || !(std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_assignable_v<T> &&
                          std::is_trivially_destructible_v<T>)) && std::is_copy_constructible_v<T>)
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
    requires ((!!fm_ASAN || !(std::is_trivially_move_constructible_v<T> && std::is_trivially_move_assignable_v<T> &&
                          std::is_trivially_destructible_v<T>)) && std::is_move_constructible_v<T>)
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
constexpr inplace_array<T, N>::~inplace_array() noexcept
{
    clear();
    annotate_contiguous_container(0, capacity());
}

template<typename T, usize N> constexpr T* inplace_array<T, N>::data() noexcept { return _storage.array.data; }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::data() const noexcept { return _storage.array.data; }
template<typename T, usize N> constexpr T* inplace_array<T, N>::begin() noexcept { return data(); }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::begin() const noexcept { return data(); }
template<typename T, usize N> constexpr T* inplace_array<T, N>::end() noexcept { return data() + size(); }
template<typename T, usize N> constexpr const T* inplace_array<T, N>::end() const noexcept { return data() + size(); }
template<typename T, usize N> constexpr bool inplace_array<T, N>::empty() const noexcept { return _size == 0; }
template<typename T, usize N> constexpr u32 inplace_array<T, N>::size() const noexcept { return _size; }
template<typename T, usize N> constexpr u32 inplace_array<T, N>::capacity() noexcept { return (u32)N; }

template<typename T, usize N>
template<typename... Args>
constexpr T& inplace_array<T, N>::unchecked_emplace_back(Args&&... args)
{
    if constexpr (!std::is_default_constructible_v<T> && std::is_copy_constructible_v<T>)
    {
        if (std::is_constant_evaluated() && empty())
        {
            T value(forward<Args>(args)...);
            start_storage_lifetime(value);
            return *construct_at(__builtin_addressof(unchecked_push_back(NoInit)), move(value));
        }
        else
            return *construct_at(__builtin_addressof(unchecked_push_back(NoInit)), forward<Args>(args)...);
    }
    else
        return *construct_at(__builtin_addressof(unchecked_push_back(NoInit)), forward<Args>(args)...);
}

template<typename T, usize N>
constexpr T& inplace_array<T, N>::unchecked_push_back(NoInitT)
{
    fm_debug3_assert(size() < capacity());
    if (std::is_constant_evaluated() && empty())
        start_storage_lifetime();
    annotate_contiguous_container(size(), size() + 1);
    return data()[_size++];
}

template<typename T, usize N>
constexpr void inplace_array<T, N>::clear() noexcept
{
    if constexpr (!std::is_trivially_destructible_v<T>)
        for (T* p = begin(); p != end(); ++p)
            destroy_at(p);
    annotate_contiguous_container(size(), 0);
    _size = 0;
}

} // namespace floormat
