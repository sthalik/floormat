#pragma once
#include <cstdio>
#include <string>
#include <Magnum/Math/Vector2.h>
#include <nlohmann/json.hpp>

namespace floormat::Serialize { [[noreturn]] void throw_failed_to_parse_vector2(const std::string& str); }

namespace nlohmann {

template<typename t>
requires std::is_integral_v<t>
struct adl_serializer<Magnum::Math::Vector2<t>> final
{
    static void to_json(json& j, const Magnum::Math::Vector2<t>& val)
    {
        char buf[64];
        using type = std::conditional_t<std::is_signed_v<t>, intmax_t, uintmax_t>;
        constexpr auto format_string = std::is_signed_v<t> ? "%jd x %jd" : "%ju x %ju";
        snprintf(buf, sizeof(buf), format_string, (type)val[0], (type)val[1]);
        j = buf;
    }
    static void from_json(const json& j, Magnum::Math::Vector2<t>& val)
    {
        using namespace floormat;
        std::string str = j;
        using type = std::conditional_t<std::is_signed_v<t>, intmax_t, uintmax_t>;
        constexpr auto format_string = std::is_signed_v<t> ? "%jd x %jd%n" : "%ju x %ju%n";
        type x = 0, y = 0;
        int n = 0;
        int ret = std::sscanf(str.data(), format_string, &x, &y, &n);
        if (ret != 2 || (size_t)n != str.size() || x != (t)x || y != (t)y)
            floormat::Serialize::throw_failed_to_parse_vector2(str);
        val = { (t)x, (t)y };
    }
};

} // namespace nlohmann
