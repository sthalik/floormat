#pragma once
#include "enum-bitset-fwd.hpp"
#include <bitset>

namespace floormat {

template<typename Enum, Enum COUNT_>
requires (std::is_enum_v<Enum> && std::is_same_v<size_t, std::common_type_t<size_t, std::underlying_type_t<Enum>>>)
struct enum_bitset : std::bitset<size_t(COUNT_)> {
    using enum_type = Enum;
    using value_type = std::underlying_type_t<enum_type>;

    static constexpr auto COUNT = size_t{value_type(COUNT_)};

    using std::bitset<COUNT>::bitset;
    constexpr bool operator[](Enum x) const { return std::bitset<COUNT>::operator[](size_t{value_type(x)}); }
    constexpr decltype(auto) operator[](Enum x) { return std::bitset<COUNT>::operator[](size_t{value_type(x)}); }
};

} // namespace floormat
