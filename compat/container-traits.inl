#pragma once
#include "container-traits.hpp"
#include <cr/GrowableArray.h>

namespace floormat {

template<typename T>
void container_traits<Array<T>>::remove_suffix(Array<T>& c, usize count)
{
    arrayRemoveSuffix(c, count);
}

} // namespace floormat
