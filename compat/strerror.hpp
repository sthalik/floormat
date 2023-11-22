#pragma once
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringView.h>

namespace floormat {

StringView get_error_string(ArrayView<char> buf);

} // namespace floormat
