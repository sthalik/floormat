#pragma once

namespace floormat {

#ifdef _MSC_VER
#ifdef _WIN64
typedef unsigned __int64   size_t;
typedef __int64            ptrdiff_t;
typedef __int64            intptr_t;
typedef unsigned __int64   uintptr_t;
#else
typedef unsigned int       size_t;
typedef int                ptrdiff_t;
typedef int                intptr_t;
typedef unsigned int       uintptr_t;
#endif
typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef long long          intmax_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long long uintmax_t;
#elif __GNUG__
typedef __SIZE_TYPE__     size_t;
typedef __PTRDIFF_TYPE__  ptrdiff_t;
typedef __INTPTR_TYPE__   intptr_t;
typedef __UINTPTR_TYPE__  uintptr_t;
typedef __INT8_TYPE__     int8_t;
typedef __INT16_TYPE__    int16_t;
typedef __INT32_TYPE__    int32_t;
typedef __INT64_TYPE__    int64_t;
typedef __UINT8_TYPE__    uint8_t;
typedef __UINT16_TYPE__   uint16_t;
typedef __UINT32_TYPE__   uint32_t;
typedef __UINT64_TYPE__   uint64_t;
typedef __INTMAX_TYPE__   intmax_t;
typedef __UINTMAX_TYPE__  uintmax_t;
#else
#include <cstddef>
#include <cstdint>
using ::std::size_t;
using ::std::ptrdiff_t;
using ::std::intptr_t;
using ::std::uintptr_t;
using ::std::int8_t;
using ::std::int16_t;
using ::std::int32_t;
using ::std::int64_t;
using ::std::intmax_t;
using ::std::uint8_t;
using ::std::uint16_t;
using ::std::uint32_t;
using ::std::uint64_t;
using ::std::uintmax_t;
#endif

} // namespace floormat
