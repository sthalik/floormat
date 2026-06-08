#pragma once
#include "compat/assert.hpp"
#if !defined __GNUG__ && !defined _MSC_VER
#include <utility>
#endif

namespace floormat {

template<typename T> T failwith(const char* str) { fm_abort("%s", str); }

} // namespace floormat
