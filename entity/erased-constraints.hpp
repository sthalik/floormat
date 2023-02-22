#pragma once
#include <cstddef>
#include <cmath>
#include <limits>
#include <cstdint>
#include <Corrade/Containers/StringView.h>

namespace floormat::entities::erased_constraints {

static_assert(sizeof(std::size_t) == sizeof(std::uintptr_t));
static_assert(sizeof(std::size_t) == sizeof(std::ptrdiff_t));

struct range final
{
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
    static_assert(sizeof(T) <= sizeof(std::size_t));
    using limits = std::numeric_limits<T>;

    if (type == type_none)
        return { limits::min(), limits::max() };
    else
    {
        if constexpr (std::is_integral_v<T> && std::is_signed_v<T>)
        {
            fm_assert(type == type_int);
            return { T(min.i), T(max.i) };
        }
        else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>)
        {
            fm_assert(type == type_uint);
            return { T(min.u), T(max.u) };
        }
        else
        {
            fm_assert(type == type_float);
            return { T(min.i), T(max.i) };
        }
    }
}

constexpr bool operator==(const range& a, const range& b)
{
    if (a.type != b.type)
        return false;

    constexpr float eps = 1e-6f;

    switch (a.type)
    {
    default: return false;
    case range::type_none:  return true;
    case range::type_float: return std::fabs(a.min.f - b.min.f) < eps && std::fabs(a.max.f - b.max.f) < eps;
    case range::type_uint:  return a.min.u == b.min.u && a.max.u == b.max.u;
    case range::type_int:   return a.min.i == b.min.i && a.max.i == b.max.i;
    }
}

struct max_length final {
    std::size_t value = std::numeric_limits<std::size_t>::max();
    constexpr operator std::size_t() const { return value; }
    //constexpr bool operator==(const max_length&) const noexcept = default;
};

struct group final {
    StringView group_name;
    constexpr operator StringView() const { return group_name; }
    constexpr group() = default;
    constexpr group(StringView name) : group_name{name} {}
    //constexpr bool operator==(const group&) const noexcept = default;
};

} // namespace floormat::entities::erased_constraints
