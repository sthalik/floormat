#include "valgrind.hpp"
#if __has_include(<valgrind/valgrind.h>)
#include <valgrind/valgrind.h>
#endif

namespace floormat {

bool is_valgrind()
{
#ifdef RUNNING_ON_VALGRIND
    static const bool is_valgrind = RUNNING_ON_VALGRIND;
#else
    static const bool is_valgrind = false;
#endif
    return is_valgrind;
}

} // namespace floormat
