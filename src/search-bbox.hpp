#pragma once
#include "search.hpp"
#include <Magnum/Math/Vector2.h>
#include <Magnum/DimensionTraits.h>

namespace floormat::Search {

template<typename T> struct bbox
{
    static_assert(std::is_arithmetic_v<T>);

    VectorTypeFor<2, T> min, max;

    constexpr bool operator==(const bbox<T>&) const = default;

    template<typename U>
    requires std::is_arithmetic_v<U>
    explicit constexpr operator bbox<U>() const {
        using Vec = VectorTypeFor<2, U>;
        return bbox<U>{ Vec(min), Vec(max) };
    }
};

} // namespace floormat::Search
