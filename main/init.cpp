#include "floormat/main.hpp"

#ifdef _WIN32
#include <sdkddkver.h>
#if _WIN32_WINNT >= 0x0A00
#include <windows.h>
#include <shellscalingapi.h>
#define FLOORMAT_CAN_SET_WIN32_DPI_AWARENESS
#endif
#endif

namespace {

void set_dpi()
{
#ifdef FLOORMAT_CAN_SET_WIN32_DPI_AWARENESS
    BOOL success = SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!success)
        DBG << "set_dpi: failed to set SetProcessDpiAwarenessContext to DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2";
#endif
}

} // namespace

void floormat::floormat_main::init_pre()
{
    set_dpi();
}
