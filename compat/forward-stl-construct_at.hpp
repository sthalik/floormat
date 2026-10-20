#pragma once
#include <version>

#if defined(_LIBCPP_VERSION) && __has_include(<__memory/construct_at.h>) && __has_include(<__memory/destroy.h>)
#  include <__memory/construct_at.h>
#  include <__memory/destroy.h>
#elif defined(__GLIBCXX__) && __has_include(<bits/stl_construct.h>)
#  include <bits/stl_construct.h>
#elif defined(_MSVC_STL_VERSION) && __has_include(<xmemory>)
#  include <xmemory>
#else
#  include <memory>
#endif
