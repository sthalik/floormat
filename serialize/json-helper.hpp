#pragma once
#include <nlohmann/json_fwd.hpp>

namespace std::filesystem { class path; }

namespace floormat {

struct json_helper final {
    using json = nlohmann::json;
    using fspath = std::filesystem::path;

    template<typename T> static T from_json(const fspath& pathname);
    template<typename T> static void to_json(const T& self, const fspath& pathname, int indent = 1);
    static json from_json_(const fspath& pathname);
    static void to_json_(const json& j, const fspath& pathname, int indent);
};

template<typename T> T json_helper::from_json(const fspath& pathname) { return from_json_(pathname); }
template<typename T> void json_helper::to_json(const T& self, const fspath& pathname, int indent) { to_json_(json(self), pathname, indent); }

} // namespace floormat
