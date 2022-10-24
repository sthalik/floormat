#pragma once

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
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#else
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
#endif

namespace std {
using ::size_t;
using ::ptrdiff_t;
using ::intptr_t;
using ::uintptr_t;

using ::int8_t;
using ::int16_t;
using ::int32_t;
using ::int64_t;
using ::uint8_t;
using ::uint16_t;
using ::uint32_t;
using ::uint64_t;
} // namespace std
