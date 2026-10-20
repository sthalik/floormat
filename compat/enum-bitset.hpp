#pragma once
#include "enum-bitset-fwd.hpp"
#include "compat/assert.hpp"

namespace floormat {

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<size_t, std::common_type_t<size_t, std::underlying_type_t<Enum>>>)
struct enum_bitset {
    using enum_type = Enum;
    using value_type = std::underlying_type_t<enum_type>;

    static constexpr auto COUNT = size_t{value_type(COUNT_)};
    static_assert(COUNT > 0);

    class reference {
        uint64_t* word;
        uint64_t mask;
        friend enum_bitset;
        constexpr reference(uint64_t& word, uint64_t mask) noexcept : word{&word}, mask{mask} {}
    public:
        constexpr reference& operator=(bool x) noexcept { *word = x ? *word | mask : *word & ~mask; return *this; }
        constexpr reference& operator=(const reference& x) noexcept { return *this = bool(x); }
        constexpr operator bool() const noexcept { return (*word & mask) != 0; }
    };

    constexpr enum_bitset() noexcept = default;
    constexpr enum_bitset(uint64_t bits) noexcept { words[0] = COUNT < 64 ? bits & ((uint64_t{1} << COUNT) - 1) : bits; }

    constexpr bool operator[](Enum x) const noexcept
    {
        const auto i = size_t{value_type(x)};
        fm_debug_assert(i < COUNT);
        return ((words[i / 64] >> (i % 64)) & 1) != 0;
    }

    constexpr reference operator[](Enum x) noexcept
    {
        const auto i = size_t{value_type(x)};
        fm_debug_assert(i < COUNT);
        return {words[i / 64], uint64_t{1} << (i % 64)};
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
    uint64_t words[(COUNT + 63) / 64] = {};
};

} // namespace floormat
