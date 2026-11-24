#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__iterator/iterator_traits.h>) && __has_include(<__iterator/concepts.h>) && \
    __has_include(<__iterator/iter_move.h>) && __has_include(<__iterator/iter_swap.h>)
#  include <__iterator/concepts.h>
#  include <__iterator/iter_move.h>
#  include <__iterator/iter_swap.h>
#  include <__iterator/iterator_traits.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/stl_iterator_base_types.h>) && __has_include(<bits/iterator_concepts.h>)
#  include <bits/iterator_concepts.h>
#  include <bits/stl_iterator_base_types.h>
#elif defined(_MSVC_STL_VERSION) && __has_include(<xutility>)
#  include <xutility>
#else
#  include <iterator>
#endif
