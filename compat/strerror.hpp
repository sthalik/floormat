#pragma once
#include <cr/ArrayView.h>

namespace floormat {

StringView get_error_string(ArrayView<char> buf, int error);
StringView get_error_string(ArrayView<char> buf);

#ifdef _WIN32
StringView get_win32_error_string(ArrayView<char> buf, u32 error);
#endif

} // namespace floormat
