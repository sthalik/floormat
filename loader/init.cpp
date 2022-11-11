#include "impl.hpp"
#ifdef _WIN32
#include <windows.h>
extern "C" __declspec(dllimport) long WINAPI RtlGetVersion (PRTL_OSVERSIONINFOEXW);
#ifdef _MSC_VER
#pragma comment(lib, "ntdll.lib")
#endif
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
    if (check_windows_build_number(10, 0, 17035))
    {
        (void)::SetConsoleOutputCP(CP_UTF8);
        (void)::SetConsoleCP(CP_UTF8);
    }
#endif
}

void system_init()
{
    static bool once = false;
    if (once)
        return;
    once = true;
    system_init_();
}

} // namespace floormat::loader_detail
