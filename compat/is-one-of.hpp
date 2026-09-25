#pragma once
#include <type_traits>

namespace floormat {

template<typename T, typename... Ts>
constexpr inline bool is_one_of = (std::is_same_v<T, Ts> || ...);

template<typename... Ts>
constexpr inline bool types_are_distinct = true;

template<typename T, typename... Ts>
constexpr inline bool types_are_distinct<T, Ts...> = !is_one_of<T, Ts...> && types_are_distinct<Ts...>;

} // namespace floormat
