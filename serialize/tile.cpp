#include "src/tile.hpp"
#include "src/chunk.hpp"
#include "serialize/tile.hpp"
#include "serialize/tile-atlas.hpp"
#include <nlohmann/json.hpp>

namespace floormat {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile_image, atlas, variant)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile, ground_image, wall_north, wall_west, passability)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(local_coords, x, y)

} // namespace floormat

using namespace floormat;

namespace nlohmann {

void adl_serializer<tile_image>::to_json(json& j, const tile_image& val) { using nlohmann::to_json; if (val.atlas) to_json(j, val); else j = nullptr; }
void adl_serializer<tile_image>::from_json(const json& j, tile_image& val) { using nlohmann::from_json; if (j.is_null()) val = {}; else from_json(j, val); }

void adl_serializer<tile>::to_json(json& j, const tile& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<tile>::from_json(const json& j, tile& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<chunk>::to_json(json& j, const chunk& val) { using nlohmann::to_json; to_json(j, val.tiles()); }
void adl_serializer<chunk>::from_json(const json& j, chunk& val) { using nlohmann::from_json; from_json(j, val.tiles()); }

void adl_serializer<local_coords>::to_json(json& j, const local_coords& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<local_coords>::from_json(const json& j, local_coords& val) { using nlohmann::from_json; from_json(j, val); }

} // namespace nlohmann
