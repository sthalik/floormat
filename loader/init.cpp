#include "impl.hpp"

#ifdef _WIN32
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#if __has_include(<ntddk.h>)
#include <ntddk.h>
#else
extern "C" __declspec(dllimport) long WINAPI RtlGetVersion(PRTL_OSVERSIONINFOEXW);
#endif
#ifdef _MSC_VER
#pragma comment(lib, "ntdll.lib")
#endif
#if defined __GNUG__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#endif // _WIN32

#ifdef __GLIBCXX__
#include <exception>
#endif

namespace floormat::loader_detail {

#ifdef _WIN32
static bool check_windows_build_number(unsigned major, unsigned minor, unsigned build)
{
    if (RTL_OSVERSIONINFOEXW rovi = {.dwOSVersionInfoSize = sizeof(rovi)}; !RtlGetVersion(&rovi))
        return rovi.dwMajorVersion > major ||
               rovi.dwMajorVersion == major && rovi.dwMinorVersion > minor ||
               rovi.dwMajorVersion == major && rovi.dwMinorVersion == minor && rovi.dwBuildNumber >= build;
    else
        return false;
}

static void init_stdio()
{
    // Text mode writes \n as \r\n, which GNU Bash keeps in $(...) and read.
    // Without a std handle, _fileno() is -2 and _setmode() fails quietly.
    std::fflush(stdout);
    std::fflush(stderr);
    for (FILE* s : { stdin, stdout, stderr })
        (void)::_setmode(::_fileno(s), _O_BINARY);
}
#endif

static void system_init_()
{
#ifdef _WIN32
    init_stdio();
    (void)::SetConsoleOutputCP(CP_UTF8);
    if (check_windows_build_number(10, 0, 17035))
        (void)::SetConsoleCP(CP_UTF8);
#endif
#ifdef __GLIBCXX__
    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#endif
}

void loader_impl::system_init()
{
    static bool once = false; // todo thread safety
    if (once)
        return;
    once = true;
    system_init_();
}

} // namespace floormat::loader_detail
