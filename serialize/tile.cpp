#include "serialize/tile.hpp"
#include "src/tile.hpp"
#include "src/global-coords.hpp"
#include "serialize/ground-atlas.hpp"
#include "src/ground-atlas.hpp"
#include <tuple>
#include <nlohmann/json.hpp>

namespace floormat {

using nlohmann::json;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(tile_image_proto, atlas, variant)

inline void to_json(json& j, const tile_image_ref& val) { j = tile_image_proto(val); }
inline void from_json(const json& j, tile_image_ref& val) { val = tile_image_proto(j); }

struct local_coords_ final {
    uint8_t x, y;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(local_coords_, x, y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(chunk_coords, x, y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(chunk_coords_, x, y, z)

inline void to_json(json& j, global_coords coord) { j = std::tuple<chunk_coords, local_coords, int8_t>{ coord.chunk(), coord.local(), coord.z() }; }
inline void from_json(const json& j, global_coords& coord) { std::tuple<chunk_coords, local_coords, int8_t> t = j; auto [ch, pos, z] = t; coord = { ch, pos, z }; }

} // namespace floormat

using namespace floormat;

namespace nlohmann {

void adl_serializer<tile_image_ref>::to_json(json& j, const tile_image_ref& val) { using nlohmann::to_json; if (val.atlas) to_json(j, val); else j = nullptr; }
void adl_serializer<tile_image_ref>::from_json(const json& j, tile_image_ref& val) { using nlohmann::from_json; if (j.is_null()) val = {}; else from_json(j, val); }

void adl_serializer<local_coords>::to_json(json& j, const local_coords& val) { using nlohmann::to_json; to_json(j, local_coords_{val.x, val.y}); }
void adl_serializer<local_coords>::from_json(const json& j, local_coords& val) { using nlohmann::from_json; local_coords_ proxy{}; from_json(j, proxy); val = {proxy.x, proxy.y}; }

void adl_serializer<chunk_coords>::to_json(json& j, const chunk_coords& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<chunk_coords>::from_json(const json& j, chunk_coords& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<chunk_coords_>::to_json(json& j, const chunk_coords_& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<chunk_coords_>::from_json(const json& j, chunk_coords_& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<global_coords>::to_json(json& j, const global_coords& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<global_coords>::from_json(const json& j, global_coords& val) { using nlohmann::from_json; from_json(j, val); }


} // namespace nlohmann
