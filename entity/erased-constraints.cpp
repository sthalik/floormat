#include "erased-constraints.hpp"
#include "compat/assert.hpp"
#include <cstdint>
#include <cmath>
#include <limits>

static_assert(sizeof(std::size_t) == sizeof(std::uintptr_t));
static_assert(sizeof(std::size_t) == sizeof(std::ptrdiff_t));

namespace floormat::entities::erased_constraints {

template<typename T> std::pair<T, T> range::convert() const
{
    static_assert(sizeof(T) <= sizeof(std::size_t));
    using limits = std::numeric_limits<T>;

    if (type == type_none)
        return { limits::min(), limits::max() };
    else
    {
        if constexpr (std::is_integral_v<T>)
        {
            if (std::is_signed_v<T>)
            {
                fm_assert(type == type_int);
                return { T(min.i), T(max.i) };
            }
            else
            {
                fm_assert(type == type_uint);
                return { T(min.u), T(max.u) };
            }
        }
        else
        {
            static_assert(std::is_floating_point_v<T>);
            fm_assert(type == type_float);
            return { T(min.f), T(max.f) };
        }
    }
}

template std::pair<std::uint8_t, std::uint8_t> range::convert() const;
template std::pair<std::uint16_t, std::uint16_t> range::convert() const;
template std::pair<std::uint32_t, std::uint32_t> range::convert() const;
template std::pair<std::uint64_t, std::uint64_t> range::convert() const;
template std::pair<std::int8_t, std::int8_t> range::convert() const;
template std::pair<std::int16_t, std::int16_t> range::convert() const;
template std::pair<std::int32_t, std::int32_t> range::convert() const;
template std::pair<std::int64_t, std::int64_t> range::convert() const;
template std::pair<float, float> range::convert() const;
template std::pair<double, double> range::convert() const;

bool operator==(const range& a, const range& b)
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

} // namespace floormat::entities::erased_constraints
