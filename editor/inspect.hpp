#pragma once
//#include "entity/accessor.hpp"
namespace floormat::entities { struct erased_accessor; }

namespace floormat {

template<typename T> void inspect_field(const void* datum, const entities::erased_accessor& accessor);

} // namespace floormat
