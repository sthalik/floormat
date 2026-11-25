#pragma once
#include "stl/iterator.hpp"
#include <cr/StringView.h>
#include <compare>
#include <concepts>
#include <initializer_list>
#include <type_traits>

namespace floormat { template<typename I> class reverse_iterator; }

namespace floormat::detail::sorted_string_map {

struct Strings {
    char* string_buffer;
    u32* offsets_array;
    u32* lengths_array;
    u32 string_buffer_length, string_buffer_capacity;
    u32 array_size, array_capacity;

    Strings() noexcept;
    Strings(const Strings& other);
    Strings(Strings&& other) noexcept;
    Strings& operator=(const Strings& other);
    Strings& operator=(Strings&& other) noexcept;
    ~Strings() noexcept;

    void swap(Strings& other) noexcept;
    void reserve(u32 reserve_size);
    u32 find_lower_bound(StringView s) const;
    u32 find_lower_bound(StringView s, u32 hint) const;
    u32 find(StringView s) const;
    StringView at(u32 index) const;
    void insert(u32 index, StringView s);
    void remove(u32 first, u32 last);
};

template<typename V>
struct reference
{
    StringView first;
    V& second;
};

} // namespace floormat::detail::sorted_string_map

namespace floormat {

// std::map's API without allocators, comparators and node handles. Insertion and erasure invalidate iterators.
// Each T needs an explicit instantiation in a .cpp that includes sorted-string-map.inl.
template<typename T>
class sorted_string_map
{
    detail::sorted_string_map::Strings _strings;
    T* _values = nullptr;

    template<typename U, typename Pred> friend u32 erase_if(sorted_string_map<U>& m, Pred&& pred);

    static T* allocate(u32 count);
    static void deallocate(T* values);
    static void relocate(T* dst, T* src, u32 count);
    void insert_at(u32 index, StringView key, T&& value);

public:
    class iterator;

    class const_iterator
    {
        const sorted_string_map* _map = nullptr;
        u32 _index = 0;

        const_iterator(const sorted_string_map* map, u32 index) noexcept;

        friend sorted_string_map;
        friend iterator;

    public:
        using container_type = sorted_string_map;
        using iterator_concept = std::random_access_iterator_tag;
        using iterator_category = std::random_access_iterator_tag;
        using value_type = detail::sorted_string_map::reference<const T>;
        using reference = value_type;
        using difference_type = isize;

        struct pointer
        {
            reference ref;

            const reference* operator->() const noexcept;
        };

        const_iterator() noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;
        reference operator[](difference_type n) const noexcept;
        const_iterator& operator++() noexcept;
        const_iterator operator++(int) noexcept;
        const_iterator& operator--() noexcept;
        const_iterator operator--(int) noexcept;
        const_iterator& operator+=(difference_type n) noexcept;
        const_iterator& operator-=(difference_type n) noexcept;
        const_iterator operator+(difference_type n) const noexcept;
        const_iterator operator-(difference_type n) const noexcept;
        difference_type operator-(const const_iterator& other) const noexcept;
        bool operator==(const const_iterator& other) const noexcept;
        std::strong_ordering operator<=>(const const_iterator& other) const noexcept;
    };

    class iterator: public const_iterator
    {
        iterator(sorted_string_map* map, u32 index) noexcept;

        friend sorted_string_map;

    public:
        using value_type = detail::sorted_string_map::reference<T>;
        using reference = value_type;
        using difference_type = isize;

        struct pointer
        {
            reference ref;

            const reference* operator->() const noexcept;
        };

        iterator() noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;
        reference operator[](difference_type n) const noexcept;
        iterator& operator++() noexcept;
        iterator operator++(int) noexcept;
        iterator& operator--() noexcept;
        iterator operator--(int) noexcept;
        iterator& operator+=(difference_type n) noexcept;
        iterator& operator-=(difference_type n) noexcept;
        iterator operator+(difference_type n) const noexcept;
        // MSVC 19.53 ignores [namespace.udecl]/11 when the hiding member is declared before the using-declaration.
        using const_iterator::operator-;
        iterator operator-(difference_type n) const noexcept;
    };

    using key_type = StringView;
    using mapped_type = T;
    using value_type = detail::sorted_string_map::reference<T>;
    using size_type = u32;
    using difference_type = isize;
    using reference = detail::sorted_string_map::reference<T>;
    using const_reference = detail::sorted_string_map::reference<const T>;
    using pointer = iterator::pointer;
    using const_pointer = const_iterator::pointer;
    using reverse_iterator = floormat::reverse_iterator<iterator>;
    using const_reverse_iterator = floormat::reverse_iterator<const_iterator>;

    sorted_string_map() noexcept;
    sorted_string_map(const sorted_string_map& other) requires std::is_copy_constructible_v<T>;
    sorted_string_map(sorted_string_map&& other) noexcept;
    sorted_string_map(std::initializer_list<Pair<StringView, T>> list) requires std::is_copy_constructible_v<T>;
    sorted_string_map& operator=(const sorted_string_map& other) requires std::is_copy_constructible_v<T>;
    sorted_string_map& operator=(sorted_string_map&& other) noexcept;
    sorted_string_map& operator=(std::initializer_list<Pair<StringView, T>> list)
        requires std::is_copy_constructible_v<T>;
    ~sorted_string_map() noexcept;

    T& at(StringView key);
    const T& at(StringView key) const;
    T& operator[](StringView key) requires std::is_default_constructible_v<T>;

    iterator begin() noexcept;
    const_iterator begin() const noexcept;
    const_iterator cbegin() const noexcept;
    iterator end() noexcept;
    const_iterator end() const noexcept;
    const_iterator cend() const noexcept;
    reverse_iterator rbegin() noexcept;
    const_reverse_iterator rbegin() const noexcept;
    const_reverse_iterator crbegin() const noexcept;
    reverse_iterator rend() noexcept;
    const_reverse_iterator rend() const noexcept;
    const_reverse_iterator crend() const noexcept;

    bool empty() const noexcept;
    u32 size() const noexcept;
    u32 max_size() const noexcept;
    void reserve(u32 count);
    void clear() noexcept;

    Pair<iterator, bool> insert(const Pair<StringView, T>& value) requires std::is_copy_constructible_v<T>;
    Pair<iterator, bool> insert(Pair<StringView, T>&& value);
    iterator insert(const_iterator hint, const Pair<StringView, T>& value) requires std::is_copy_constructible_v<T>;
    iterator insert(const_iterator hint, Pair<StringView, T>&& value);
    template<typename P> Pair<iterator, bool> insert(P&& value)
        requires requires(P& p) { p.first(); p.second(); } || requires(P& p) { p.first; p.second; };
    template<typename P> iterator insert(const_iterator hint, P&& value)
        requires requires(P& p) { p.first(); p.second(); } || requires(P& p) { p.first; p.second; };
    template<typename I> void insert(I first, I last);
    void insert(std::initializer_list<Pair<StringView, T>> list) requires std::is_copy_constructible_v<T>;
    template<typename R> void insert_range(R&& r);
    template<typename M> Pair<iterator, bool> insert_or_assign(StringView key, M&& obj);
    template<typename M> iterator insert_or_assign(const_iterator hint, StringView key, M&& obj);
    template<typename... Args> Pair<iterator, bool> emplace(StringView key, Args&&... args);
    template<typename... Args> iterator emplace_hint(const_iterator hint, StringView key, Args&&... args);
    template<typename... Args> Pair<iterator, bool> try_emplace(StringView key, Args&&... args);
    template<typename... Args> iterator try_emplace(const_iterator hint, StringView key, Args&&... args);

    iterator erase(iterator pos);
    iterator erase(const_iterator pos);
    iterator erase(const_iterator first, const_iterator last);
    u32 erase(StringView key);
    void swap(sorted_string_map& other) noexcept;

    u32 count(StringView key) const;
    iterator find(StringView key);
    const_iterator find(StringView key) const;
    bool contains(StringView key) const;
    iterator lower_bound(StringView key);
    const_iterator lower_bound(StringView key) const;
    iterator upper_bound(StringView key);
    const_iterator upper_bound(StringView key) const;
    Pair<iterator, iterator> equal_range(StringView key);
    Pair<const_iterator, const_iterator> equal_range(StringView key) const;

    bool operator==(const sorted_string_map& other) const
        requires requires(const T& x) { { x == x } -> std::convertible_to<bool>; };
};

template<typename I>
    requires std::same_as<I, typename I::container_type::iterator> ||
             std::same_as<I, typename I::container_type::const_iterator>
I operator+(isize n, const I& it) noexcept;
template<typename T> void swap(sorted_string_map<T>& a, sorted_string_map<T>& b) noexcept;
template<typename T, typename Pred> u32 erase_if(sorted_string_map<T>& m, Pred&& pred);

} // namespace floormat
