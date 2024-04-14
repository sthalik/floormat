#pragma once

namespace floormat {

template<typename Base, typename Derived>
constexpr inline bool is_strict_base_of = std::is_base_of_v<Base, Derived> && !std::is_same_v<Base, Derived>;

} // namespace floormat
