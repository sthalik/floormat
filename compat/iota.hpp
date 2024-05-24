#pragma once
#include <array>

namespace floormat::detail {

template<typename Type, size_t Count>
std::array<Type, Count> constexpr iota_array_()
{
    static_assert( size_t(Type(Count)) == Count );
    std::array<Type, Count> ret;
    for (size_t i = 0; i < Count; i++)
        ret[i] = Type(i);
    return ret;
}

} // namespace floormat::detail

namespace floormat {

template<typename Type, size_t Count>
constexpr inline std::array<Type, Count> iota_array = detail::iota_array_<Type, Count>();

} // namespace floormat
