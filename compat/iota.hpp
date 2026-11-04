#pragma once
#include <array>

namespace floormat::detail {

template<typename Type, usize Count>
std::array<Type, Count> constexpr iota_array_()
{
    static_assert( usize(Type(Count)) == Count );
    std::array<Type, Count> ret;
    for (usize i = 0; i < Count; i++)
        ret[i] = Type(i);
    return ret;
}

} // namespace floormat::detail

namespace floormat {

template<typename Type, usize Count>
constexpr inline std::array<Type, Count> iota_array = detail::iota_array_<Type, Count>();

} // namespace floormat
