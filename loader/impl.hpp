#pragma once
#include <Corrade/Containers/StringView.h>

namespace floormat::Serialize { struct anim; }

namespace floormat::loader_detail {

bool chdir(StringView pathname);
Serialize::anim deserialize_anim(StringView filename);

} // namespace floormat::loader_detail
