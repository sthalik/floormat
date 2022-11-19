#pragma once
#include <cmath>
#include <type_traits>
#include <utility>
#include <algorithm>
#include <Corrade/Containers/StringView.h>

namespace floormat::entities::erased_constraints {

static_assert(sizeof(std::size_t) == sizeof(std::uintptr_t));
static_assert(sizeof(std::size_t) == sizeof(std::ptrdiff_t));

struct range final {
    using U = std::size_t;
    using I = std::make_signed_t<U>;
    enum type_ : unsigned char { type_none, type_float, type_uint, type_int, };
    union element {
        float f;
        U u;
        I i;
    };

    element min {.i = 0}, max {.i = 0};
    type_ type = type_none;

    template<typename T> constexpr std::pair<T, T> convert() const;
};

template<typename T> constexpr std::pair<T, T> range::convert() const
{
    using limits = std::numeric_limits<T>;
    switch (type) {
    case type_float:
        if constexpr (limits::is_integer())
            return { T(std::floor(min.f)), T(std::ceil(max.f)) };
        else
            return { min.f, max.f };
    case type_uint:  return { std::max(T(min.u), limits::min()), std::min(T(max.u), limits::max()) };
    case type_int:   return { std::max(T(min.i), limits::min()), std::min(T(max.i), limits::max()) };
    default: case type_none:  return { limits::min(), limits::max() };
    }
}

struct length final {
    std::size_t value = (std::size_t)-1;
};

struct group final {
    StringView group_name;
};

} // namespace floormat::entities::erased_constraints

namespace floormat::entities::constraints {

template<typename T> struct range {
    using limits = std::numeric_limits<T>;
    T min = limits::min(), max = limits::max();

    constexpr operator erased_constraints::range() const;
};

template<typename T> constexpr range<T>::operator erased_constraints::range() const
{
    using enum erased_constraints::range::type_;
    if constexpr (std::is_floating_point_v<T>)
        return { { .f = min }, { .f = max }, type_float };
    if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
        return { {.u = min}, {.u = max}, type_uint };
    if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
        return { {.i = min}, {.i = max}, type_int };
    return { {}, {}, type_none };
}

using length = erased_constraints::length;
using group = erased_constraints::group;

} // namespace floormat::entities::constraints
