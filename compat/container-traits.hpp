#pragma once

namespace floormat {

template<typename C> struct container_traits;

template<typename T>
struct container_traits<Array<T>>
{
    static void remove_suffix(Array<T>& c, usize count);
};

} // namespace floormat
