#pragma once
#include <Corrade/Containers/StringView.h>
#include <Magnum/Math/Vector4.h>

namespace floormat::entities::erased_constraints {

struct range final
{
    using U = size_t;
    using I = std::make_signed_t<U>;
    enum type_ : unsigned char {
        type_none,
        type_float, type_uint, type_int,
        type_float4, type_uint4, type_int4,
    };
    union element {
        float f;
        U u;
        I i = 0;
        Math::Vector4<float> f4;
        Math::Vector4<U> u4;
        Math::Vector4<I> i4;
    };

    element min, max;
    type_ type = type_none;

    template<typename T> std::pair<T, T> convert() const;
    friend bool operator==(const range& a, const range& b);
};

struct max_length final {
    size_t value = size_t(-1);
    constexpr operator size_t() const { return value; }
};

} // namespace floormat::entities::erased_constraints
