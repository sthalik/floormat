#include "magnum-vector.hpp"
#include "compat/format.hpp"
#include "compat/exception.hpp"
#include <Magnum/Math/Vector.h>
#include <cstdio>
#include <string>
#include <nlohmann/json.hpp>

namespace floormat::Serialize {

namespace {

[[noreturn]] void throw_failed_to_parse_vector2(const std::string& str)
{
    fm_throw("failed to parse Vector2 '{}'"_cf, str);
}

[[noreturn]] void throw_vector2_overflow(const std::string& str)
{
    fm_throw("numeric overflow in Vector2 '{}'"_cf, str);
}

using nlohmann::json;
using Math::Vector;
using Math::Vector2;
using Math::Vector3;
using Math::Vector4;

template<size_t N, typename T>
struct vec_serializer
{
    static void to_json(json& j, Math::Vector<N, T> val)
    {
        std::array<T, N> array{};
        for (auto i = 0uz; i < N; i++)
            array[i] = val[i];
        using nlohmann::to_json;
        to_json(j, array);
    }

    static void from_json(const json& j, Math::Vector<N, T>& val)
    {
        std::array<T, N> array{};
        using nlohmann::from_json;
        from_json(j, array);
        for (auto i = 0uz; i < N; i++)
            val[i] = array[i];
    }
};

template<typename T>
struct vec2_serializer
{
    static void to_json(json& j, Vector<2, T> val)
    {
        char buf[64];
        using type = std::conditional_t<std::is_signed_v<T>, intmax_t, uintmax_t>;
        constexpr auto format_string = std::is_signed_v<T> ? "%jd x %jd" : "%ju x %ju";
        snprintf(buf, sizeof(buf), format_string, (type)val[0], (type)val[1]);
        j = buf;
    }

    static void from_json(const json& j, Vector<2, T>& val)
    {
        using namespace floormat;
        std::string str = j;
        using type = std::conditional_t<std::is_signed_v<T>, intmax_t, uintmax_t>;
        constexpr auto format_string = std::is_signed_v<T> ? "%jd x %jd%n" : "%ju x %ju%n";
        type x = 0, y = 0;
        int n = 0;
        int ret = std::sscanf(str.data(), format_string, &x, &y, &n);
        if (ret != 2 || (size_t)n != str.size() || x != (T)x || y != (T)y)
            throw_failed_to_parse_vector2(str);
        if constexpr(sizeof(T) < sizeof(type))
            if (x != (type)(T)x || y != (type)(T)y)
                throw_vector2_overflow(str);
        val = { (T)x, (T)y };
    }
};

} // namespace
} // namespace floormat::Serialize

namespace nlohmann {

using floormat::Serialize::vec_serializer;
using floormat::Serialize::vec2_serializer;
using floormat::size_t;
using namespace Magnum::Math;

template<size_t N, typename T>
void adl_serializer<Vector<N, T>>::to_json(json& j, Vector<N, T> val)
{
    if constexpr(N != 2 || !std::is_integral_v<T>)
        vec_serializer<N, T>::to_json(j, val);
    else
        vec2_serializer<T>::to_json(j, val);
}

template<size_t N, typename T>
void adl_serializer<Vector<N, T>>::from_json(const json& j, Vector<N, T>& val)
{
    if constexpr(N != 2 || !std::is_integral_v<T>)
        vec_serializer<N, T>::from_json(j, val);
    else
        vec2_serializer<T>::from_json(j, val);
}

#define FM_INST_VEC_SERIALIZER_1(N, Type)               \
    template struct adl_serializer< Vector<N, Type>>;   \
    template struct adl_serializer< Vector##N<Type>>

#define FM_INST_VEC_SERIALIZER(Type)                    \
    FM_INST_VEC_SERIALIZER_1(Type,    float);           \
    FM_INST_VEC_SERIALIZER_1(Type,   double);           \
    FM_INST_VEC_SERIALIZER_1(Type,   int8_t);           \
    FM_INST_VEC_SERIALIZER_1(Type,  int16_t);           \
    FM_INST_VEC_SERIALIZER_1(Type,  int32_t);           \
    FM_INST_VEC_SERIALIZER_1(Type,  int64_t);           \
    FM_INST_VEC_SERIALIZER_1(Type,  uint8_t);           \
    FM_INST_VEC_SERIALIZER_1(Type, uint16_t);           \
    FM_INST_VEC_SERIALIZER_1(Type, uint32_t);           \
    FM_INST_VEC_SERIALIZER_1(Type, uint64_t)

FM_INST_VEC_SERIALIZER(2);
FM_INST_VEC_SERIALIZER(3);
FM_INST_VEC_SERIALIZER(4);

} // namespace nlohmann
