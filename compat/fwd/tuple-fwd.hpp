#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__fwd/tuple.h>)
#  include <__fwd/tuple.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/stl_pair.h>)
#  include <bits/stl_pair.h>
#elif defined(_MSVC_STL_VERSION)
#  include <utility>
#else
#  include <tuple>
#endif
