#if defined _MSC_VER && defined _DEBUG
#include <cstdlib>
#include <cstring>
#include <crtdbg.h>
#include <windows.h>

#pragma warning(suppress: 4073)
#pragma init_seg(lib)

namespace {

int abort_on_crt_error(int report_type, char* message, int*)
{
    if (report_type == _CRT_WARN)
        return FALSE;
    else
    {
        DWORD num_bytes_written;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, (DWORD)std::strlen(message), &num_bytes_written,
                  nullptr);
        OutputDebugStringA(message);
        std::abort();
    }
}

[[maybe_unused]] const bool is_abort_behavior_set = [] {
    // Fail fast as in Release, without the message box.
    _set_abort_behavior(_CALL_REPORTFAULT, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    // Failed CRT assertions and errors open a dialog otherwise.
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, abort_on_crt_error);
    return true;
}();

} // namespace
#endif
