#include "magnum-vector2i.hpp"
#include "compat/exception.hpp"
#include <nlohmann/json.hpp>

using namespace floormat;

void floormat::Serialize::throw_failed_to_parse_vector2(const std::string& str)
{
    fm_throw("failed to parse Vector2 '{}'"_cf, str);
}

void floormat::Serialize::throw_vector2_overflow(const std::string& str)
{
    fm_throw("numeric overflow in Vector2 '{}'"_cf, str);
}

using namespace nlohmann;

template<typename T>
requires std::is_integral_v<T>
void nlohmann::adl_serializer<Magnum::Math::Vector2<T>>::to_json(json& j, const Magnum::Math::Vector2<T>& val)
{
    char buf[64];
    using type = std::conditional_t<std::is_signed_v<T>, intmax_t, uintmax_t>;
    constexpr auto format_string = std::is_signed_v<T> ? "%jd x %jd" : "%ju x %ju";
    snprintf(buf, sizeof(buf), format_string, (type)val[0], (type)val[1]);
    j = buf;
}

template<typename T>
requires std::is_integral_v<T>
void nlohmann::adl_serializer<Magnum::Math::Vector2<T>>::from_json(const json& j, Magnum::Math::Vector2<T>& val)
{
    using namespace floormat;
    std::string str = j;
    using type = std::conditional_t<std::is_signed_v<T>, intmax_t, uintmax_t>;
    constexpr auto format_string = std::is_signed_v<T> ? "%jd x %jd%n" : "%ju x %ju%n";
    type x = 0, y = 0;
    int n = 0;
    int ret = std::sscanf(str.data(), format_string, &x, &y, &n);
    if (ret != 2 || (size_t)n != str.size() || x != (T)x || y != (T)y)
        floormat::Serialize::throw_failed_to_parse_vector2(str);
    if constexpr(sizeof(T) < sizeof(type))
        if (x != (type)(T)x || y != (type)(T)y)
            floormat::Serialize::throw_vector2_overflow(str);
    val = { (T)x, (T)y };
}

template struct nlohmann::adl_serializer<Magnum::Math::Vector2<uint8_t>>;
template struct nlohmann::adl_serializer<Magnum::Math::Vector2<uint16_t>>;
template struct nlohmann::adl_serializer<Magnum::Math::Vector2<uint32_t>>;
template struct nlohmann::adl_serializer<Magnum::Math::Vector2<uint64_t>>;
template struct nlohmann::adl_serializer<Magnum::Math::Vector2<int8_t>>;
template struct nlohmann::adl_serializer<Magnum::Math::Vector2<int16_t>>;
template struct nlohmann::adl_serializer<Magnum::Math::Vector2<int32_t>>;
template struct nlohmann::adl_serializer<Magnum::Math::Vector2<int64_t>>;
