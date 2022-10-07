#pragma once
#include <bitset>

namespace Magnum::Examples {

template<typename Enum>
struct enum_bitset : std::bitset<(std::size_t)Enum::MAX> {
    static_assert(std::is_same_v<std::size_t, std::common_type_t<std::size_t, std::underlying_type_t<Enum>>>);
    static_assert(std::is_same_v<Enum, std::decay_t<Enum>>);
    using std::bitset<(std::size_t)Enum::MAX>::bitset;
    constexpr bool operator[](Enum x) const { return operator[]((std::size_t)x); }
    constexpr decltype(auto) operator[](Enum x) {
        return std::bitset<(std::size_t)Enum::MAX>::operator[]((std::size_t)x);
    }
};

} // namespace Magnum::Examples
