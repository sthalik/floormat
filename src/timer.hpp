#pragma once
#include "timer-fwd.hpp"
#include <mg/Time.h> // todo! replace with my own

namespace floormat {

constexpr inline size_t fm_DATETIME_BUF_SIZE = 32;
const char* format_datetime_to_string(char(&buf)[fm_DATETIME_BUF_SIZE]);

} // namespace floormat
