#include "world.hpp"
#include "tile.hpp"
#include "global-coords.hpp"
#include <nlohmann/json.hpp>

namespace floormat {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(chunk_coords, x, y)

struct global_coords_ final {
    chunk_coords chunk;
    local_coords local;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(global_coords_, chunk, local)

} // namespace floormat

namespace nlohmann {

using namespace floormat;

void adl_serializer<chunk_coords>::to_json(json& j, const chunk_coords& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<chunk_coords>::from_json(const json& j, chunk_coords& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<global_coords>::to_json(json& j, const global_coords& val) { using nlohmann::to_json; to_json(j, global_coords_{val.chunk(), val.local()}); }
void adl_serializer<global_coords>::from_json(const json& j, global_coords& val) { using nlohmann::from_json; global_coords_ x; from_json(j, x); val = {x.chunk, x.local}; }

} // namespace nlohmann
