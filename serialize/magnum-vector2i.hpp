#pragma once
#include "compat/exception.hpp"
#include <cstdio>
#include <string>
#include <Magnum/Math/Vector2.h>
#include <nlohmann/json.hpp>

namespace nlohmann {

template<typename t>
requires std::is_integral_v<t>
struct adl_serializer<Magnum::Math::Vector2<t>> final
{
    static void to_json(json& j, const Magnum::Math::Vector2<t>& val)
    {
        char buf[64];
        using type = std::conditional_t<std::is_signed_v<t>, std::intmax_t, std::uintmax_t>;
        constexpr auto format_string = std::is_signed_v<t> ? "%jd x %jd" : "%ju x %ju";
        snprintf(buf, sizeof(buf), format_string, (type)val[0], (type)val[1]);
        j = buf;
    }
    static void from_json(const json& j, Magnum::Math::Vector2<t>& val)
    {
        using namespace floormat;
        std::string str = j;
        using type = std::conditional_t<std::is_signed_v<t>, std::intmax_t, std::uintmax_t>;
        constexpr auto format_string = std::is_signed_v<t> ? "%jd x %jd%n" : "%ju x %ju%n";
        type x = 0, y = 0;
        int n = 0;
        int ret = std::sscanf(str.data(), format_string, &x, &y, &n);
        if (ret != 2 || (std::size_t)n != str.size() || x != (t)x || y != (t)y)
            fm_throw("failed to parse Vector2 '{}'"_cf, str);
        val = { (t)x, (t)y };
    }
};

} // namespace nlohmann
