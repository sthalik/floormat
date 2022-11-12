#pragma once

#include "src/anim.hpp"
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<>
struct adl_serializer<floormat::anim_frame> {
    static void to_json(json& j, const floormat::anim_frame& val);
    static void from_json(const json& j, floormat::anim_frame& val);
};

template<>
struct adl_serializer<floormat::anim_group> {
    static void to_json(json& j, const floormat::anim_group& val);
    static void from_json(const json& j, floormat::anim_group& val);
};

template<>
struct adl_serializer<floormat::anim_def> {
    static void to_json(json& j, const floormat::anim_def& val);
    static void from_json(const json& j, floormat::anim_def& val);
};

} // namespace nlohmann
