#include "setenv.hpp"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <Corrade/Containers/String.h>
#include <Corrade/Containers/StringIterable.h>

namespace floormat {

#ifdef _WIN32
int setenv(const char* name, const char* value, int overwrite)
{
    if (!std::strchr(name, '=') || !value || !*value)
    {
        errno = EINVAL;
        return -1;
    }
    if (!overwrite)
        if (const auto* s = std::getenv(name); s && *s)
            return 0;

    return _putenv("="_s.join(StringIterable{name, value}).data());
}

int unsetenv(const char* name)
{
    if (!name || !*name)
    {
        errno = EINVAL;
        return -1;
    }

    return _putenv("="_s.join(StringIterable{name, ""_s}).data());
}
#endif

} // namespace floormat
