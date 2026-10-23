#include "strerror.hpp"
#include <cerrno>
#include <string.h>
#include <cr/StringView.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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

#ifdef _WIN32
StringView get_win32_error_string(ArrayView<char> buf, uint32_t error)
{
    DWORD n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
                             MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), buf.data(), (DWORD)buf.size(), nullptr);
    while (n > 0 && (buf[n-1] == '\r' || buf[n-1] == '\n' || buf[n-1] == ' '))
        n--;
    if (n > 0)
    {
        buf[n] = '\0';
        return { buf.data(), (size_t)n };
    }
    return "Unknown error"_s;
}
#endif

} // namespace floormat
