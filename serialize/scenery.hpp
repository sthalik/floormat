#pragma once
#include "src/scenery.hpp"
#include <Corrade/Containers/String.h>
#include <nlohmann/json_fwd.hpp>

namespace floormat { struct serialized_scenery; }

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
};

template<> struct adl_serializer<floormat::serialized_scenery> {
    static void to_json(json& j, const floormat::serialized_scenery& val);
    static void from_json(const json& j, floormat::serialized_scenery& val);
};

} // namespace nlohmann
