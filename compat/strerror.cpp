#include "strerror.hpp"
#include <cerrno>
#include <string.h>
#include <Corrade/Containers/StringView.h>

namespace floormat {

StringView get_error_string(ArrayView<char> buf, int error)
{
#ifdef _WIN32
    ::strerror_s(buf.data(), buf.size(), error);
    if (buf[0])
        return { buf.data() };
#elif defined __GLIBC__ && !((_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE)
    char* str { ::strerror_r(error, buf.data(), buf.size()) };
    if (str)
        return str;
#else
    int status { ::strerror_r(error, buf.data(), buf.size()) };
    if (status == 0)
        return { buf.data() };
#endif
    return "Unknown error"_s;
};

StringView get_error_string(ArrayView<char> buf)
{
    return get_error_string(buf, errno);
}

} // namespace floormat
