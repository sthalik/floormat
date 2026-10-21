#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__chrono/duration.h>) && __has_include(<__chrono/steady_clock.h>) && \
    __has_include(<__chrono/system_clock.h>) && __has_include(<__chrono/time_point.h>)
#  include <__chrono/duration.h>
#  include <__chrono/steady_clock.h>
#  include <__chrono/system_clock.h>
#  include <__chrono/time_point.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/chrono.h>)
#  include <bits/chrono.h>
#else
// MSVC's <__msvc_chrono.hpp> lacks chrono_literals.
#  include <chrono>
#endif
