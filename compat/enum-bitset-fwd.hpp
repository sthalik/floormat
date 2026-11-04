#pragma once

namespace floormat {

template<typename Enum, Enum COUNT_ = Enum::COUNT>
requires (std::is_enum_v<Enum> && std::is_same_v<usize, std::common_type_t<usize, std::underlying_type_t<Enum>>>)
struct enum_bitset;

} // namespace floormat
