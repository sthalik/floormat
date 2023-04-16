#pragma once
#include "src/tile-atlas.hpp"
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace nlohmann {

template<>
struct adl_serializer<std::shared_ptr<floormat::tile_atlas>> final {
    static void to_json(json& j, const std::shared_ptr<const floormat::tile_atlas>& x);
    static void from_json(const json& j, std::shared_ptr<floormat::tile_atlas>& x);
};

} // namespace nlohmann
