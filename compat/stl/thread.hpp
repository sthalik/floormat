#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__thread/thread.h>) && __has_include(<__thread/this_thread.h>)
#  include <__thread/thread.h>
#  include <__thread/this_thread.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/std_thread.h>) && __has_include(<bits/this_thread_sleep.h>)
#  include <bits/std_thread.h>
#  include <bits/this_thread_sleep.h>
#else
#  include <thread>
#endif
