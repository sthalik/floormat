#pragma once
#include <mg/Vector.h>

namespace floormat {

template<typename T>
requires (std::is_same_v<Int, T> || std::is_same_v<float, T>)
constexpr bool rect_intersects(Math::Vector<2, T> min1, Math::Vector<2, T> max1, Math::Vector<2, T> min2, Math::Vector<2, T> max2)
{
    return min1.data()[0] < max2.data()[0] && max1.data()[0] > min2.data()[0] &&
           min1.data()[1] < max2.data()[1] && max1.data()[1] > min2.data()[1];
}

} // namespace floormat
