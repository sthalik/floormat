#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__exception/exception.h>)
#  include <__exception/exception.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/exception.h>)
#  include <bits/exception.h>
#else
#  include <exception>
#endif
