#include "erased-constraints.hpp"
#include "compat/assert.hpp"
#include <cstdint>
#include <cmath>
#include <limits>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

static_assert(sizeof(std::size_t) == sizeof(std::uintptr_t));
static_assert(sizeof(std::size_t) == sizeof(std::ptrdiff_t));

namespace floormat::entities::erased_constraints {

template<typename T> std::pair<T, T> range::convert() const
{
    static_assert(sizeof(T) <= sizeof(min));

    if (type == type_none)
    {
        if constexpr (is_magnum_vector<T>)
        {
            using U = typename T::Type;
            constexpr auto Size = T::Size;
            T a, b;
            for (auto i = 0_uz; i < Size; i++)
                a[i] = std::numeric_limits<U>::min(), b[i] = std::numeric_limits<U>::max();
            return {a, b};
        }
        else
            return { std::numeric_limits<T>::min(), std::numeric_limits<T>::max() };
    }
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
                if (type == type_int)
                {
                    fm_assert(min.i >= 0 && max.i >= 0);
                    return { T(min.i), T(max.i) };
                }
                else
                {
                    fm_assert(type == type_uint);
                    return { T(min.u), T(max.u) };
                }
            }
        }
        else if constexpr(is_magnum_vector<T>)
        {
            using U = typename T::Type;
            constexpr auto Size = T::Size;
            static_assert(Size >= 2 && Size <= 4);
            T a, b;
            if constexpr(std::is_integral_v<U>)
            {
                if constexpr(std::is_signed_v<U>)
                {
                    fm_assert(type == type_int4);
                    for (auto i = 0_uz; i < Size; i++)
                        a[i] = U(min.i4[i]), b[i] = U(max.i4[i]);
                }
                else
                {
                    if (type == type_int4)
                    {
                        for (auto i = 0_uz; i < Size; i++)
                        {
                            fm_assert(min.i4[i] >= 0 && max.i4[i] >= 0);
                            a[i] = U(min.i4[i]), b[i] = U(max.i4[i]);
                        }
                    }
                    else
                    {
                        fm_assert(type == type_uint4);
                        for (auto i = 0_uz; i < Size; i++)
                            a[i] = U(min.u4[i]), b[i] = U(max.u4[i]);
                    }
                }
            }
            else
            {
                static_assert(std::is_floating_point_v<U>);
                fm_assert(type == type_float4);
                for (auto i = 0_uz; i < Size; i++)
                    a[i] = U(min.f4[i]), b[i] = U(max.f4[i]);
            }
            return { a, b };
        }
        else
        {
            static_assert(std::is_floating_point_v<T>);
            fm_assert(type == type_float);
            return { T(min.f), T(max.f) };
        }
    }
}

template<typename T> using pair2 = std::pair<T, T>;

template pair2<std::uint8_t> range::convert() const;
template pair2<std::uint16_t> range::convert() const;
template pair2<std::uint32_t> range::convert() const;
template pair2<std::uint64_t> range::convert() const;
template pair2<std::int8_t> range::convert() const;
template pair2<std::int16_t> range::convert() const;
template pair2<std::int32_t> range::convert() const;
template pair2<float> range::convert() const;
template pair2<Vector2ub> range::convert() const;
template pair2<Vector2us> range::convert() const;
template pair2<Vector2ui> range::convert() const;
template pair2<Vector2b> range::convert() const;
template pair2<Vector2s> range::convert() const;
template pair2<Vector2i> range::convert() const;
template pair2<Vector2> range::convert() const;
template pair2<Vector3ub> range::convert() const;
template pair2<Vector3us> range::convert() const;
template pair2<Vector3ui> range::convert() const;
template pair2<Vector3b> range::convert() const;
template pair2<Vector3s> range::convert() const;
template pair2<Vector3i> range::convert() const;
template pair2<Vector3> range::convert() const;
template pair2<Vector4ub> range::convert() const;
template pair2<Vector4us> range::convert() const;
template pair2<Vector4ui> range::convert() const;
template pair2<Vector4b> range::convert() const;
template pair2<Vector4s> range::convert() const;
template pair2<Vector4i> range::convert() const;
template pair2<Vector4> range::convert() const;

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
