#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__algorithm/lower_bound.h>)
#  include <__algorithm/lower_bound.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/stl_algo.h>)
#  include <bits/stl_algo.h>
#elif defined(_MSVC_STL_VERSION) && __has_include(<xutility>)
#  include <xutility>
#else
#  include <algorithm>
#endif
