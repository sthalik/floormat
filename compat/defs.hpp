#pragma once

#ifdef _MSC_VER
#   define fm_FUNCTION_NAME __FUNCSIG__
#else
#   define fm_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#define fm_begin(...) [&]{__VA_ARGS__}()

#define fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(type)                 \
    [[deprecated]] type(const type&) noexcept = default;            \
    [[deprecated]] type& operator=(const type&) noexcept = default

#define fm_DECLARE_DEFAULT_COPY_ASSIGNMENT(type)                    \
    constexpr type(const type&) noexcept = default;                 \
    constexpr type& operator=(const type&) noexcept = default

#define fm_DECLARE_DELETED_COPY_ASSIGNMENT(type)                    \
    type(const type&) = delete;                                     \
    type& operator=(const type&) = delete

#define fm_DECLARE_DELETED_MOVE_ASSIGNMENT(type)                    \
    [[deprecated]] type(type&&) = delete;                           \
    [[deprecated]] type& operator=(type&&) = delete

#define fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT(type)                    \
    constexpr type(type&&) noexcept = default;                      \
    constexpr type& operator=(type&&) noexcept = default

#define fm_DECLARE_DEFAULT_MOVE_COPY_ASSIGNMENTS(type)              \
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT(type);                                                      \
    fm_DECLARE_DEFAULT_COPY_ASSIGNMENT(type)
