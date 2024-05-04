#pragma once

#ifdef _MSC_VER
#   define fm_FUNCTION_NAME __FUNCSIG__
#else
#   define fm_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#define fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(type)                                 \
    [[deprecated, maybe_unused]] type(const type&) noexcept = default;              \
    [[deprecated, maybe_unused]] type& operator=(const type&) noexcept = default

#define fm_DECLARE_DEFAULT_COPY_ASSIGNMENT(type)                                    \
    [[maybe_unused]] constexpr type(const type&) noexcept = default;                \
    [[maybe_unused]] constexpr type& operator=(const type&) noexcept = default

#define fm_DECLARE_DEFAULT_COPY_ASSIGNMENT_(type)                                   \
    [[maybe_unused]] type(const type&) noexcept = default;                          \
    [[maybe_unused]] type& operator=(const type&) noexcept = default

#define fm_DECLARE_DELETED_COPY_ASSIGNMENT(type)                                    \
    type(const type&) = delete;                                                     \
    type& operator=(const type&) = delete

#define fm_DECLARE_DELETED_MOVE_ASSIGNMENT(type)                                    \
    type(type&&) = delete;                                                          \
    type& operator=(type&&) = delete

#define fm_DECLARE_DEPRECATED_MOVE_ASSIGNMENT(type)                                 \
    [[deprecated, maybe_unused]] type(type&&) = default;                            \
    [[deprecated, maybe_unused]] type& operator=(type&&) = default

#define fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT(type)                                    \
    [[maybe_unused]] constexpr type(type&&) noexcept = default;                     \
    [[maybe_unused]] constexpr type& operator=(type&&) noexcept = default

#define fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(type)                                   \
    [[maybe_unused]] type(type&&) noexcept = default;                               \
    [[maybe_unused]] type& operator=(type&&) noexcept = default

#define fm_DECLARE_DEFAULT_MOVE_COPY_ASSIGNMENTS(type)                              \
    [[maybe_unused]] fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT(type);                      \
    [[maybe_unused]] fm_DECLARE_DEFAULT_COPY_ASSIGNMENT(type)

#define fm_DECLARE_DEFAULT_MOVE_COPY_ASSIGNMENTS_(type)                             \
    [[maybe_unused]] fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(type);                     \
    [[maybe_unused]] fm_DECLARE_DEFAULT_COPY_ASSIGNMENT_(type)

#define fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(type)                              \
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(type);                                       \
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(type)

#ifdef _MSC_VER
#   define fm_noinline __declspec(noinline)
#else
#   define fm_noinline __attribute__((noinline))
#endif

#ifdef _MSC_VER
// standard version not supported in MSVC v14x ABI
// see https://devblogs.microsoft.com/cppblog/msvc-cpp20-and-the-std-cpp20-switch/
#define fm_no_unique_address msvc::no_unique_address
#else
#define fm_no_unique_address no_unique_address
#endif

#ifdef _MSC_VER
#define fm_UNROLL _Pragma("loop(ivdep)")
#define fm_UNROLL_4 fm_UNROLL
#define fm_UNROLL_8 fm_UNROLL
#define fm_UNROLL_2 fm_UNROLL
#else
#ifndef __SIZEOF_POINTER__
#error "missing __SIZEOF_POINTER__"
#endif
#define fm_UNROLL_4 _Pragma("GCC unroll 4")
#define fm_UNROLL_8 _Pragma("GCC unroll 8")
#if __SIZEOF_POINTER__ > 4
#define fm_UNROLL _Pragma("GCC unroll 8")
#else
#define fm_UNROLL _Pragma("GCC unroll 4")
#endif
#endif

#ifdef __SANITIZE_ADDRESS__
#define fm_ASAN 1
#elif defined __has_feature
#if __has_feature(address_sanitizer)
#define fm_ASAN 1
#endif
#endif
#ifndef fm_ASAN
#define fm_ASAN 0
#endif

#ifndef fm_FILENAME_MAX
#define fm_FILENAME_MAX (260)
#endif
