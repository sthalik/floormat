#pragma once
#include <cr/ArrayView.h>

namespace floormat {

StringView get_error_string(ArrayView<char> buf, int error);
StringView get_error_string(ArrayView<char> buf);

} // namespace floormat
