#pragma once

#ifdef _WIN32
#include <malloc.h>
#ifdef _MSC_VER
#define alloca _alloca
#endif
#else
#include <alloca.h>
#endif
