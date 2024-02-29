#pragma once
#include "timer-fwd.hpp"
#include <mg/Time.h>

namespace floormat {

long double operator/(Ns a, Ns b) noexcept;
using namespace Magnum::Math::Literals::TimeLiterals;

constexpr inline size_t fm_DATETIME_BUF_SIZE = 32;
const char* format_datetime_to_string(char(&buf)[fm_DATETIME_BUF_SIZE]);

} // namespace floormat
