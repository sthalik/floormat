#include "impl.hpp"

#ifdef _WIN32
#include <cstdio>
#include <windows.h>
#if __has_include(<ntddk.h>)
#include <ntddk.h>
#else
extern "C" __declspec(dllimport) long WINAPI RtlGetVersion(PRTL_OSVERSIONINFOEXW);
#endif
#ifdef _MSC_VER
#pragma comment(lib, "ntdll.lib")
#endif
#if defined __GNUG__ && !defined __clang__
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
#endif

static void system_init_()
{
#ifdef _WIN32
    (void)::SetConsoleOutputCP(CP_UTF8);
    if (check_windows_build_number(10, 0, 17035))
    {
        for (const auto h : { STD_INPUT_HANDLE, STD_ERROR_HANDLE, STD_OUTPUT_HANDLE })
        {
            HANDLE handle = GetStdHandle(h);
            if (!handle)
            {
                puts(""); // put breakpoint here
            }
            else
            {
                DWORD mode = 0;
                if (!::GetConsoleMode(handle, &mode))
                {
                    puts(""); // put breakpoint here
                }
                else
                {
                    if (!::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
                    {
                        puts(""); // put breakpoint here
                    }
                }
            }
        }
        (void)::SetConsoleCP(CP_UTF8);
    }
#endif
#ifdef __GLIBCXX__
    std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#endif
}

void loader_impl::system_init()
{
    static bool once = false;
    if (once)
        return;
    once = true;
    system_init_();
}

} // namespace floormat::loader_detail
