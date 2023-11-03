#pragma once
#include "src/wall-atlas.hpp"
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<>
struct adl_serializer<std::shared_ptr<floormat::wall_atlas>>
{
    static void to_json(json& j, const std::shared_ptr<const floormat::wall_atlas>& x);
    static void from_json(const json& j, std::shared_ptr<floormat::wall_atlas>& x);
};

} // namespace nlohmann
