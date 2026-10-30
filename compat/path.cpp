#include "path.hpp"
#include <cr/Pair.h>
#ifdef _WIN32
#include "strerror.hpp"
#include <cr/Array.h>
#include <cr/Debug.h>
#include <cr/Unicode.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cr/Optional.h>
#include <cr/Path.h>
#endif

namespace floormat {

namespace {

bool is_separator(char c)
{
#ifdef _WIN32
    return c == '/' || c == '\\';
#else
    return c == '/';
#endif
}

#ifdef _WIN32

size_t component_length(StringView s)
{
    size_t i = 0;
    while (i < s.size() && !is_separator(s[i]))
        i++;
    return i;
}

size_t share_length(StringView s)
{
    const auto server = component_length(s);
    if (server == 0 || server == s.size())
        return 0;
    const auto share = component_length(s.exceptPrefix(server + 1));
    return share ? server + 1 + share : 0;
}

bool is_unc_keyword(StringView s)
{
    return s.size() >= 4 && (s[0] | 0x20) == 'u' && (s[1] | 0x20) == 'n' && (s[2] | 0x20) == 'c' &&
           is_separator(s[3]);
}

bool has_drive(StringView s)
{
    return s.size() >= 2 && (((s[0] | 0x20) >= 'a' && (s[0] | 0x20) <= 'z')) && s[1] == ':';
}

// The longest path Windows supports, 32767 UTF-16 code units, plus the null terminator. The buffer
// takes 64 kB, so it comes from the heap.
constexpr DWORD path_capacity = 1 << 15;

String narrow_path(ArrayView<const wchar_t> path)
{
    String ret = Utility::Unicode::narrow(path);
    for (char& c : ret)
        if (c == '\\')
            c = '/';
    return ret;
}

// Must run before anything else can overwrite GetLastError().
String fail(StringView what)
{
    char buf[256];
    ERR << what << get_win32_error_string(buf, GetLastError());
    return {};
}

#endif

size_t root_length(StringView pathname)
{
    size_t n = 0;
#ifdef _WIN32
    if (pathname.size() >= 4 && is_separator(pathname[0]) && is_separator(pathname[1]) &&
        (pathname[2] == '?' || pathname[2] == '.') && is_separator(pathname[3]))
    {
        n = 4;
        const auto rest = pathname.exceptPrefix(n);
        if (is_unc_keyword(rest))
        {
            if (const auto len = share_length(rest.exceptPrefix(4)))
                n += 4 + len;
        }
        else
            n += component_length(rest);
    }
    else if (pathname.size() >= 2 && is_separator(pathname[0]) && is_separator(pathname[1]))
        n = 2 + share_length(pathname.exceptPrefix(2));
    else if (has_drive(pathname))
        n = 2;
#endif
    while (n < pathname.size() && is_separator(pathname[n]))
        n++;
    return n;
}

} // namespace

Pair<StringView, StringView> split_pathname(StringView pathname)
{
    const auto root = root_length(pathname);
    size_t tail = pathname.size();
    while (tail > root && !is_separator(pathname[tail - 1]))
        tail--;
    size_t head = tail;
    while (head > root && is_separator(pathname[head - 1]))
        head--;
    return {pathname.prefix(head), pathname.exceptPrefix(tail)};
}

#ifdef _WIN32

String extended_length_path(StringView pathname)
{
    if (pathname.hasPrefix("//?/"_s))
        return pathname;
    if (pathname.hasPrefix("//./"_s))
        return "//?/"_s + pathname.exceptPrefix(4);
    if (pathname.hasPrefix("//"_s))
        return "//?/UNC/"_s + pathname.exceptPrefix(2);
    return "//?/"_s + pathname;
}

String current_directory()
{
    Array<wchar_t> buf{NoInit, path_capacity};
    // Returns the length without the null terminator, or the size needed with it if buf is too small.
    const DWORD size = GetCurrentDirectoryW(path_capacity, buf.data());
    if (!size || size >= path_capacity)
        return fail("GetCurrentDirectoryW():"_s);
    return narrow_path(buf.prefix(size));
}

String executable_location()
{
    Array<wchar_t> buf{NoInit, path_capacity};
    // Returns the length without the null terminator, or path_capacity if the path got truncated.
    const DWORD size = GetModuleFileNameW(nullptr, buf.data(), path_capacity);
    if (!size || size == path_capacity)
        return fail("GetModuleFileNameW():"_s);
    return narrow_path(buf.prefix(size));
}

#else

String current_directory()
{
    if (auto path = Path::currentDirectory())
        return move(*path);
    return {};
}

String executable_location()
{
    if (auto path = Path::executableLocation())
        return move(*path);
    return {};
}

#endif

} // namespace floormat
