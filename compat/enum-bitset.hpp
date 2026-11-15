#pragma once
#include "enum-bitset-fwd.hpp"
#include "assert.hpp"

namespace floormat {

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
struct enum_bitset {
    using enum_type = Enum;
    using value_type = std::underlying_type_t<enum_type>;

    static constexpr auto COUNT = usize{value_type(COUNT_)};
    static_assert(COUNT > 0);

    class reference {
        usize* word;
        usize mask;
        friend enum_bitset;
        constexpr reference(usize& word, usize mask) noexcept : word{&word}, mask{mask} {}
    public:
        constexpr reference& operator=(bool x) noexcept { *word = x ? *word | mask : *word & ~mask; return *this; }
        constexpr reference& operator=(const reference& x) noexcept { return *this = bool(x); }
        constexpr operator bool() const noexcept { return (*word & mask) != 0; }
    };

    constexpr enum_bitset() noexcept = default;
    explicit constexpr enum_bitset(usize bits) noexcept { words[0] = COUNT < sizeof(usize)*8 ? bits & ((1uz << COUNT) - 1) : bits; }

    constexpr bool operator[](Enum x) const noexcept
    {
        const auto i = usize{value_type(x)};
        fm_debug_assert(i < COUNT);
        return ((words[i / (sizeof(usize)*8)] >> (i % (sizeof(usize)*8))) & 1) != 0;
    }

    constexpr reference operator[](Enum x) noexcept
    {
        const auto i = usize{value_type(x)};
        fm_debug_assert(i < COUNT);
        return {words[i / (sizeof(usize)*8)], 1uz << (i % (sizeof(usize)*8))};
    }

    constexpr void reset() noexcept
    {
        for (auto& w : words)
            w = 0;
    }

    constexpr bool any() const noexcept
    {
        for (auto w : words)
            if (w != 0)
                return true;
        return false;
    }

private:
    usize words[(COUNT + sizeof(usize)*8 - 1) / (sizeof(usize)*8)] = {};
};

} // namespace floormat
