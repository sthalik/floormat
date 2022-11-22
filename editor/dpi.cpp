#ifdef _WIN32
#include "app.hpp"
#include <windows.h>

namespace floormat {

void app::set_dpi_aware()
{
    SetProcessDPIAware();
}

} // namespace floormat
#endif
