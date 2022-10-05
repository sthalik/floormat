#include <cstdio>
#include <string>
#include <exception>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <nlohmann/json.hpp>

namespace nlohmann {

template<typename t>
struct adl_serializer<Magnum::Math::Vector2<t>> final {
    static void to_json(json& j, const Magnum::Math::Vector2<t>& x);
    static void from_json(const json& j, Magnum::Math::Vector2<t>& x);
};

template<typename t>
void adl_serializer<Magnum::Math::Vector2<t>>::to_json(json& j, const Magnum::Math::Vector2<t>& val)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%d x %d", val[0], val[1]);
    j = buf;
}

template<typename t>
void adl_serializer<Magnum::Math::Vector2<t>>::from_json(const json& j, Magnum::Math::Vector2<t>& val)
{
    std::string str = j;
    int x = 0, y = 0, n = 0;
    int ret = std::sscanf(str.c_str(), "%d x %d%n", &x, &y, &n);
    if (ret != 2 || (std::size_t)n != str.size())
    {
        std::string msg; msg.reserve(64 + str.size());
        msg += "failed to parse string '";
        msg += str;
        msg += "' as Magnum::Vector2i";
        throw std::invalid_argument(msg);
    }
    val = { x, y };
}

} // namespace nlohmann
