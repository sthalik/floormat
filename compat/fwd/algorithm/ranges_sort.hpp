#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__algorithm/ranges_sort.h>)
#  include <__algorithm/ranges_sort.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/ranges_algo.h>)
#  if _GLIBCXX_RELEASE < 16 && __has_include(<bits/stl_algo.h>)
// before 16, ranges_algo.h calls std::stable_sort, std::push_heap etc.
// without including stl_algo.h, and before 15, ranges_algobase.h calls
// std::__memcmp, std::min etc. without including stl_algobase.h.
#    include <bits/stl_algo.h>
#  elif __has_include(<bits/stl_tempbuf.h>)
// ranges_algo.h uses _Temporary_buffer without including stl_tempbuf.h.
#    include <bits/stl_tempbuf.h>
#  endif
#  include <bits/ranges_algo.h>
#else
#  include <algorithm>
#endif
