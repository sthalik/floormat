#pragma once

namespace floormat { struct anim_def; }
namespace Corrade::Containers { template<typename T> class BasicStringView; using StringView = BasicStringView<const char>; }

namespace floormat::loader_detail {

bool chdir(StringView pathname);
anim_def deserialize_anim(StringView filename);
void system_init();

} // namespace floormat::loader_detail
