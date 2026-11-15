#pragma once
#include "assert.hpp"

namespace floormat {

template<usize N>
class bitset
{
    static_assert(N > 0);
    using word_type = std::conditional_t<(N <= 8), u8, std::conditional_t<(N <= 32), u32, usize>>;

    word_type words[(N + sizeof(word_type)*8 - 1) / (sizeof(word_type)*8)] = {};

public:
    class reference
    {
        friend bitset;

        word_type* word;
        word_type mask;

        constexpr reference(word_type& word, word_type mask) noexcept;

    public:
        constexpr reference& operator=(bool x) noexcept;
        constexpr reference& operator=(const reference& x) noexcept;
        constexpr operator bool() const noexcept;
    };

    constexpr bitset() noexcept = default;
    explicit constexpr bitset(usize bits) noexcept;

    constexpr bool operator[](usize i) const noexcept;
    constexpr reference operator[](usize i) noexcept;
    constexpr void reset() noexcept;
    constexpr bool any() const noexcept;
};

template<usize N>
constexpr bitset<N>::reference::reference(word_type& word, word_type mask) noexcept : word{&word}, mask{mask} {}

template<usize N>
constexpr auto bitset<N>::reference::operator=(bool x) noexcept -> reference&
{
    *word = x ? word_type(*word | mask) : word_type(*word & ~mask);
    return *this;
}

template<usize N>
constexpr auto bitset<N>::reference::operator=(const reference& x) noexcept -> reference&
{
    return *this = bool(x);
}

template<usize N>
constexpr bitset<N>::reference::operator bool() const noexcept
{
    return (*word & mask) != 0;
}

template<usize N>
constexpr bitset<N>::bitset(usize bits) noexcept
{
    if constexpr (N < sizeof(usize)*8)
        bits &= (1uz << N) - 1;
    words[0] = word_type(bits);
}

template<usize N>
constexpr bool bitset<N>::operator[](usize i) const noexcept
{
    fm_debug_assert(i < N);
    return ((words[i / (sizeof(word_type)*8)] >> (i % (sizeof(word_type)*8))) & 1) != 0;
}

template<usize N>
constexpr auto bitset<N>::operator[](usize i) noexcept -> reference
{
    fm_debug_assert(i < N);
    return {words[i / (sizeof(word_type)*8)], word_type(word_type{1} << (i % (sizeof(word_type)*8)))};
}

template<usize N>
constexpr void bitset<N>::reset() noexcept
{
    for (auto& w : words)
        w = 0;
}

template<usize N>
constexpr bool bitset<N>::any() const noexcept
{
    for (auto w : words)
        if (w != 0)
            return true;
    return false;
}

} // namespace floormat
