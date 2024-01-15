#pragma once

namespace floormat {

template<typename Enum, Enum COUNT_ = Enum::COUNT>
requires (std::is_enum_v<Enum> && std::is_same_v<size_t, std::common_type_t<size_t, std::underlying_type_t<Enum>>>)
struct enum_bitset;

} // namespace floormat
