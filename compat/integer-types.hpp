#pragma once

#if !defined(_MSC_VER) && !defined(__GNUG__)
#include <cstddef>
#include <cstdint>
#endif

namespace floormat {

#ifdef _MSC_VER
#ifdef _WIN64
typedef unsigned __int64   usize;
typedef __int64            isize;
typedef __int64            iptr;
typedef unsigned __int64   uptr;
#else
typedef unsigned int       usize;
typedef int                isize;
typedef int                iptr;
typedef unsigned int       uptr;
#endif
typedef signed char        i8;
typedef short              i16;
typedef int                i32;
typedef long long          i64;
typedef long long          imax;
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef unsigned long long umax;
#elif __GNUG__
typedef __SIZE_TYPE__     usize;
typedef __PTRDIFF_TYPE__  isize;
typedef __INTPTR_TYPE__   iptr;
typedef __UINTPTR_TYPE__  uptr;
typedef __INT8_TYPE__     i8;
typedef __INT16_TYPE__    i16;
typedef __INT32_TYPE__    i32;
typedef __INT64_TYPE__    i64;
typedef __UINT8_TYPE__    u8;
typedef __UINT16_TYPE__   u16;
typedef __UINT32_TYPE__   u32;
typedef __UINT64_TYPE__   u64;
typedef __INTMAX_TYPE__   imax;
typedef __UINTMAX_TYPE__  umax;
#else
using usize = ::std::size_t;
using isize = ::std::ptrdiff_t;
using iptr = ::std::intptr_t;
using uptr = ::std::uintptr_t;
using i8 = ::std::int8_t;
using i16 = ::std::int16_t;
using i32 = ::std::int32_t;
using i64 = ::std::int64_t;
using imax = ::std::intmax_t;
using u8 = ::std::uint8_t;
using u16 = ::std::uint16_t;
using u32 = ::std::uint32_t;
using u64 = ::std::uint64_t;
using umax = ::std::uintmax_t;
#endif

} // namespace floormat
