#include "serialize/magnum-vector2i.hpp"
#include "serialize/anim.hpp"

#include <tuple>
#include <filesystem>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>

namespace Magnum::Examples::Serialize {

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_frame, ground, offset, size)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim_group, name, frames, ground)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(anim, name, nframes, actionframe, fps, groups, width, height)

} // namespace Magnum::Examples::Serialize

using namespace Magnum::Examples::Serialize;

namespace nlohmann {

void adl_serializer<anim_frame>::to_json(json& j, const anim_frame& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_frame>::from_json(const json& j, anim_frame& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<anim_group>::to_json(json& j, const anim_group& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim_group>::from_json(const json& j, anim_group& val) { using nlohmann::from_json; from_json(j, val); }

void adl_serializer<anim>::to_json(json& j, const anim& val) { using nlohmann::to_json; to_json(j, val); }
void adl_serializer<anim>::from_json(const json& j, anim& val) { using nlohmann::from_json; from_json(j, val); }

} // namespace nlohmann
