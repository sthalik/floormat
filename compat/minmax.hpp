#pragma once
#include "assert.hpp"
#include <Magnum/Math/TypeTraits.h>
#include <initializer_list>

namespace Magnum::Math {

template <class T>
requires IsScalar<T>::value
constexpr T min(std::initializer_list<T> ilist)
{
    fm_assert(ilist.size() > 0);

    auto it = ilist.begin();
    T best = *it;

    for (++it; it != ilist.end(); ++it)
        if (*it < best)
            best = *it;

    return best;
}

template <class T>
requires IsScalar<T>::value
constexpr T max(std::initializer_list<T> ilist)
{
    fm_assert(ilist.size() > 0);

    auto it = ilist.begin();
    T best = *it;

    for (++it; it != ilist.end(); ++it)
        if (best < *it)
            best = *it;

    return best;
}

} // namespace Magnum::Math
