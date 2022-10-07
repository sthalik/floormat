#pragma once

#ifdef _MSC_VER
#   define FUNCTION_NAME __FUNCSIG__
#else
#   define FUNCTION_NAME __PRETTY_FUNCTION__
#endif

#define progn(...) [&]{__VA_ARGS__;}()

#define DECLARE_DEPRECATED_COPY_ASSIGNMENT(type)            \
    [[deprecated]] type(const type&) = default;    \
    [[deprecated]] type& operator=(const type&) = default

#define DECLARE_DELETED_COPY_ASSIGNMENT(type)               \
    type(const type&) = delete;                             \
    type& operator=(const type&) = delete
