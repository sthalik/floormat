#include "corrade-string.hpp"
#include <nlohmann/json.hpp>
#include <string_view>

using namespace floormat;

namespace nlohmann {

void adl_serializer<String>::to_json(json& j, const String& val)
{
    std::string_view s{val.data(), val.size()};
    using nlohmann::to_json;
    to_json(j, s);
}

void adl_serializer<String>::from_json(const json& j, String& val)
{
    using nlohmann::from_json;
    std::string_view s;
    from_json(j, s);
    val = String{s.data(), s.size()};
}

void adl_serializer<StringView>::to_json(json& j, StringView val)
{
    std::string_view s{val.data(), val.size()};
    using nlohmann::to_json;
    to_json(j, s);
}

} // namespace nlohmann
