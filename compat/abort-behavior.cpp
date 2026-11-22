#if defined _MSC_VER && defined _DEBUG
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <crtdbg.h>
#include <windows.h>

#pragma warning(suppress: 4073)
#pragma init_seg(lib)

namespace {

void break_into_debugger(int)
{
    if (IsDebuggerPresent())
        __debugbreak();
}

int abort_on_crt_report(int report_type, char* message, int* return_value)
{
    if (report_type == _CRT_WARN)
        return FALSE;
    else
    {
        DWORD num_bytes_written;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, (DWORD)std::strlen(message), &num_bytes_written,
                  nullptr);
        OutputDebugStringA(message);
        if (!IsDebuggerPresent())
            std::abort();
        // 1 makes _CrtDbgReport()'s caller break into the debugger.
        *return_value = 1;
        return TRUE;
    }
}

[[maybe_unused]] const bool is_abort_behavior_set = [] {
    _set_abort_behavior(0, _WRITE_ABORT_MSG);
    std::signal(SIGABRT, break_into_debugger);
    // Failed CRT assertions and errors open a dialog otherwise.
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, abort_on_crt_report);
    return true;
}();

} // namespace
#endif
