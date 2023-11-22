#include "strerror.hpp"
#include <errno.h>
#include <string.h>

namespace floormat {

StringView get_error_string(ArrayView<char> buf)
{
#ifndef _WIN32
    if constexpr(std::is_same_v<char*, std::decay_t<decltype(::strerror_r(errno, buf.data(), buf.size()))>>)
    {
        const char* str { ::strerror_r(errno, buf.data(), buf.size()) };
        if (str)
            return str;
    }
    else
    {
        const int status { ::strerror_r(errno, buf.data(), buf.size()) };
        if (status == 0)
            return buf;
    }
#else
    ::strerror_s(buf.data(), buf.size(), errno);
    if (buf[0])
        return buf;
#endif

    return "Unknown error"_s;
};

} // namespace floormat
