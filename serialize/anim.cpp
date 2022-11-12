#include "serialize/magnum-vector.hpp"
#include "serialize/magnum-vector2i.hpp"
#include "serialize/corrade-string.hpp"
#include "serialize/anim.hpp"

namespace floormat {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground, offset, size)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_group, name, frames, ground, offset)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_def, object_name, anim_name, pixel_size, nframes, actionframe, fps, groups, width, height)

} // namespace floormat::Serialize

using namespace floormat;

namespace nlohmann {

void adl_serializer<anim_frame>::to_json(json& j, const anim_frame& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_frame>::from_json(const json& j, anim_frame& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<anim_group>::to_json(json& j, const anim_group& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_group>::from_json(const json& j, anim_group& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<anim_def>::to_json(json& j, const anim_def& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_def>::from_json(const json& j, anim_def& val) { using nlohmann::from_json; from_json(j, val); }

} // namespace nlohmann
