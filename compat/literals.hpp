#pragma once
#include "integer-types.hpp"

#if !(defined __cpp_size_t_suffix || defined _MSC_VER && _MSVC_LANG < 202004)
#ifdef _MSC_VER
#pragma system_header
#else
#pragma GCC system_header
#endif
consteval auto operator""uz(unsigned long long int x) { return ::floormat::size_t(x); }
#endif
