#pragma once

// swl uses std::is_trivial_v, deprecated in C++26.
#ifdef __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <swl/variant.hpp>

#ifdef __GNUG__
#pragma GCC diagnostic pop
#endif
