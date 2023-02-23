#pragma once
#include <Corrade/Containers/StringView.h>
#include <Magnum/Math/Vector4.h>

namespace floormat::entities::erased_constraints {

template<typename T> struct is_magnum_vector_ final : std::false_type {};
template<std::size_t N, typename T> struct is_magnum_vector_<Math::Vector<N, T>> : std::true_type {};
template<typename T> struct is_magnum_vector_<Math::Vector2<T>> : std::true_type {};
template<typename T> struct is_magnum_vector_<Math::Vector3<T>> : std::true_type {};
template<typename T> struct is_magnum_vector_<Math::Vector4<T>> : std::true_type {};
template<typename T> constexpr inline bool is_magnum_vector = is_magnum_vector_<T>::value;

struct range final
{
    using U = std::size_t;
    using I = std::make_signed_t<U>;
    enum type_ : unsigned char {
        type_none,
        type_float, type_uint, type_int,
        type_float4, type_uint4, type_int4,
    };
    union element {
        float f;
        U u;
        I i;
        Math::Vector4<float> f4;
        Math::Vector4<U> u4;
        Math::Vector4<I> i4;
    };

    element min {.i = 0}, max {.i = 0};
    type_ type = type_none;

    template<typename T> std::pair<T, T> convert() const;
    friend bool operator==(const range& a, const range& b);
};

struct max_length final {
    std::size_t value = std::size_t(-1);
    constexpr operator std::size_t() const { return value; }
};

struct group final {
    StringView group_name;
    constexpr operator StringView() const { return group_name; }
    constexpr group() = default;
    constexpr group(StringView name) : group_name{name} {}
};

} // namespace floormat::entities::erased_constraints
