#pragma once

#ifdef _MSC_VER
#   define fm_FUNCTION_NAME __FUNCSIG__
#else
#   define fm_FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#define fm_begin(...) [&]{__VA_ARGS__}()

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
