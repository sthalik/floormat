#pragma once
#include "src/scenery.hpp"
#include <Corrade/Containers/StringView.h>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<> struct adl_serializer<floormat::scenery_type> {
    static void to_json(json& j, floormat::scenery_type val);
    static void from_json(const json& j, floormat::scenery_type& val);
};

template<> struct adl_serializer<floormat::rotation> {
    static void to_json(json& j, floormat::rotation val);
    static void from_json(const json& j, floormat::rotation& val);
};

template<> struct adl_serializer<floormat::scenery_proto> {
    static void to_json(json& j, const floormat::scenery_proto& val);
    static void from_json(const json& j, floormat::scenery_proto& val);

    struct item {
        Corrade::Containers::StringView name, description;
        floormat::scenery_proto proto;
    };

    static std::vector<item> deserialize_list(Corrade::Containers::StringView file);
};

} // namespace nlohmann
