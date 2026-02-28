#include "fix-argv0.hpp"
#include <cstring>

#ifdef _WIN32
#include <string.h>
#define my_strcasecmp _stricmp
#else
#include <strings.h>
#define my_strcasecmp strcasecmp
#endif

namespace floormat {

char* fix_argv0(char* argv0) noexcept
{
#ifdef _WIN32
    if (auto* c = std::strrchr(argv0, '\\'); c && c[1])
    {
        if (auto* s = std::strrchr(c, '.'); s && !my_strcasecmp(".exe", s))
            *s = '\0';
        return c+1;
    }
#else
    if (auto* c = std::strrchr(argv0, '/'); c && c[1])
        return c+1;
#endif
    return argv0;
}

} // namespace floormat
