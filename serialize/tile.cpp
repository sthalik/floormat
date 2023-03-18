#include "serialize/tile.hpp"
#include "src/tile.hpp"
#include "src/global-coords.hpp"
#include "serialize/tile-atlas.hpp"
#include <nlohmann/json.hpp>

namespace floormat {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile_image_proto, atlas, variant)

inline void to_json(nlohmann::json& j, const tile_image_ref& val) { j = tile_image_proto(val); }
inline void from_json(const nlohmann::json& j, tile_image_ref& val) { val = tile_image_proto(j); }

struct local_coords_ final {
    uint8_t x, y;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(local_coords_, x, y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(chunk_coords, x, y)

struct global_coords_ final {
    chunk_coords chunk;
    local_coords local;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(global_coords_, chunk, local)

} // namespace floormat

using namespace floormat;

namespace nlohmann {

void adl_serializer<tile_image_ref>::to_json(json& j, const tile_image_ref& val) { using nlohmann::to_json; if (val.atlas) to_json(j, val); else j = nullptr; }
void adl_serializer<tile_image_ref>::from_json(const json& j, tile_image_ref& val) { using nlohmann::from_json; if (j.is_null()) val = {}; else from_json(j, val); }

void adl_serializer<local_coords>::to_json(json& j, const local_coords& val) { using nlohmann::to_json; to_json(j, local_coords_{val.x, val.y}); }
void adl_serializer<local_coords>::from_json(const json& j, local_coords& val) { using nlohmann::from_json; local_coords_ proxy{}; from_json(j, proxy); val = {proxy.x, proxy.y}; }

void adl_serializer<chunk_coords>::to_json(json& j, const chunk_coords& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<chunk_coords>::from_json(const json& j, chunk_coords& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<global_coords>::to_json(json& j, const global_coords& val) { using nlohmann::to_json; to_json(j, global_coords_{val.chunk(), val.local()}); }
void adl_serializer<global_coords>::from_json(const json& j, global_coords& val) { using nlohmann::from_json; global_coords_ x; from_json(j, x); val = {x.chunk, x.local}; }


} // namespace nlohmann
