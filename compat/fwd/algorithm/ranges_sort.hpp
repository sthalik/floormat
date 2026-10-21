#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__algorithm/ranges_sort.h>)
#  include <__algorithm/ranges_sort.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/ranges_algo.h>)
// ranges_algo.h uses _Temporary_buffer without including stl_tempbuf.h.
#  if __has_include(<bits/stl_tempbuf.h>)
#    include <bits/stl_tempbuf.h>
#  endif
#  include <bits/ranges_algo.h>
#else
#  include <algorithm>
#endif
