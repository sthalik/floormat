#pragma once
#include "src/ground-atlas.hpp"
#include <nlohmann/json_fwd.hpp>

namespace floormat {

struct ground_info;

} // namespace floormat

namespace nlohmann {

template<>
struct adl_serializer<floormat::ground_def> final {
    static void to_json(json& j, const floormat::ground_def& x);
    static void from_json(const json& j, floormat::ground_def& x);
};

template<>
struct adl_serializer<std::shared_ptr<floormat::ground_atlas>> final {
    static void to_json(json& j, const std::shared_ptr<const floormat::ground_atlas>& x);
    static void from_json(const json& j, std::shared_ptr<floormat::ground_atlas>& x);
};

} // namespace nlohmann
