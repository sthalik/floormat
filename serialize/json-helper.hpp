#pragma once
#include <nlohmann/json.hpp>
#include <Corrade/Containers/StringView.h>

namespace floormat {

struct json_helper final {
    using json = nlohmann::json;

    template<typename T> static T from_json(StringView pathname);
    template<typename T> static void to_json(const T& self, StringView pathname, int indent = 1);
    static json from_json_(StringView pathname);
    static void to_json_(const json& j, StringView pathname, int indent);
};

template<typename T> T json_helper::from_json(StringView pathname) { return from_json_(pathname); }
template<typename T> void json_helper::to_json(const T& self, StringView pathname, int indent) { to_json_(json(self), pathname, indent); }

} // namespace floormat
