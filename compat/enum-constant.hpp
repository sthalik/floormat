#pragma once
#include <type_traits>

namespace floormat {

template<typename E, E value>
requires std::is_enum_v<E>
struct enum_constant {};

} // namespace floormat
