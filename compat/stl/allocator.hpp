#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__memory/allocator.h>) && __has_include(<__memory/allocator_traits.h>) && \
    __has_include(<__memory/addressof.h>) && __has_include(<__memory/align.h>)
#  include <__memory/addressof.h>
#  include <__memory/align.h>
#  include <__memory/allocator.h>
#  include <__memory/allocator_traits.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/allocator.h>) && __has_include(<bits/alloc_traits.h>) && \
    __has_include(<bits/align.h>) && __has_include(<bits/move.h>)
#  include <bits/allocator.h>
#  include <bits/align.h>
#  include <bits/alloc_traits.h>
#  include <bits/move.h>
#elif defined(_MSVC_STL_VERSION) && __has_include(<xmemory>)
#  include <xmemory>
#else
#  include <memory>
#endif
