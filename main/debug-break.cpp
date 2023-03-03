#include "floormat/main.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ptrace.h>
#include <signal.h>
#endif

void floormat::floormat_main::debug_break()
{
#ifdef _WIN32
    if (IsDebuggerPresent()) [[unlikely]]
        DebugBreak();
#else
    if (ptrace(PTRACE_TRACEME, 0, 1, 0) == -1)
        ::raise(SIGUSR1);
#endif
}
