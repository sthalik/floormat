#pragma once
#include "enum-bitset-fwd.hpp"
#include "bitset.hpp"

namespace floormat {

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
struct enum_bitset {
    using enum_type = Enum;
    using value_type = std::underlying_type_t<enum_type>;

    static constexpr auto COUNT = usize{value_type(COUNT_)};

    using reference = bitset<COUNT>::reference;

    constexpr enum_bitset() noexcept = default;
    explicit constexpr enum_bitset(usize bits) noexcept;

    constexpr bool operator[](Enum x) const noexcept;
    constexpr reference operator[](Enum x) noexcept;
    constexpr void reset() noexcept;
    constexpr bool any() const noexcept;

private:
    bitset<COUNT> bits;
};

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
constexpr enum_bitset<Enum, COUNT_>::enum_bitset(usize bits) noexcept : bits{bits} {}

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
constexpr bool enum_bitset<Enum, COUNT_>::operator[](Enum x) const noexcept
{
    return bits[usize{value_type(x)}];
}

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
constexpr auto enum_bitset<Enum, COUNT_>::operator[](Enum x) noexcept -> reference
{
    return bits[usize{value_type(x)}];
}

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
constexpr void enum_bitset<Enum, COUNT_>::reset() noexcept
{
    bits.reset();
}

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
constexpr bool enum_bitset<Enum, COUNT_>::any() const noexcept
{
    return bits.any();
}

} // namespace floormat
