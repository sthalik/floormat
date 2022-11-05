#include "string.hpp"
#include <Corrade/Containers/String.h>
#include <string_view>
#include <nlohmann/json.hpp>

using String = Corrade::Containers::String;
using StringView = Corrade::Containers::StringView;

namespace nlohmann {

void adl_serializer<String>::to_json(json& j, const String& val)
{
    using nlohmann::to_json;
    to_json(j, std::string_view { val.cbegin(), val.cend() });
}

void adl_serializer<String>::from_json(const json& j, String& val)
{
    using nlohmann::from_json;
    std::string_view str;
    from_json(j, str);
    val = { str.cbegin(), str.size() };
}

void adl_serializer<StringView>::to_json(json& j, const StringView& val)
{
    using nlohmann::to_json;
    to_json(j, std::string_view { val.cbegin(), val.cend() });
}

void adl_serializer<StringView>::from_json(const json& j, StringView& val)
{
    using nlohmann::from_json;
    std::string_view str;
    from_json(j, str);
    val = { str.cbegin(), str.size() };
}

} // namespace nlohmann
