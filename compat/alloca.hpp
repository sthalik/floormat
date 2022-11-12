#pragma once

#ifdef _WIN32
#   include <malloc.h>
#   define alloca _alloca
#else
#   include <alloca.h>
#endif
