#include "strerror.hpp"
#include <errno.h>
#include <string.h>

namespace floormat {

StringView get_error_string(ArrayView<char> buf)
{
#ifdef _WIN32
    ::strerror_s(buf.data(), buf.size(), errno);
    if (buf[0])
        return buf;
#elif defined __GLIBC__ && !((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE)
    char* str { ::strerror_r(errno, buf.data(), buf.size()) };
    if (str)
        return str;
#else
    int status { ::strerror_r(errno, buf.data(), buf.size()) };
    if (status == 0)
        return buf;
#endif

    return "Unknown error"_s;
};

} // namespace floormat
