#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__algorithm/sort.h>)
#  include <__algorithm/sort.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/stl_algo.h>)
#  include <bits/stl_algo.h>
#else
#  include <algorithm>
#endif
