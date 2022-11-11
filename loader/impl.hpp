#pragma once

namespace Corrade::Containers { template<typename T> class BasicStringView; using StringView = BasicStringView<const char>; }
namespace floormat::Serialize { struct anim; }

namespace floormat::loader_detail {

bool chdir(StringView pathname);
Serialize::anim deserialize_anim(StringView filename);
void system_init();

} // namespace floormat::loader_detail
