#pragma once
#include <type_traits>
#include <cr/ArrayView.h>

namespace floormat {

template<typename T>
constexpr void fill(ArrayView<T> dst, const std::type_identity_t<T>& value)
{
    for (T& x : dst)
        x = value;
}

} // namespace floormat
