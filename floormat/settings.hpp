#pragma once
#include "compat/defs.hpp"
#include <cr/String.h>
#include <mg/Vector2.h>

namespace floormat {

enum class driver_mode : uint8_t { off,
    all,
    coverage, // scenes useful for generating coverage data
    profile,  // scenes useful for LTO+PGO runs
};

struct fm_settings
{
    inline fm_settings() noexcept = default;
    virtual ~fm_settings() noexcept;
    fm_DEPRECATED_COPY(fm_settings);
    fm_DEFAULT_MOVE_(fm_settings);

    String title = "floormat editor"_s;
    const char* const* argv = nullptr; int argc = 0;
    Magnum::Math::Vector2<int> resolution{1024, 720};
    uint32_t fixed_framerate = 0;   // 0 = feed update() the measured frame time
    // Passes over the scene table. Sampling by restart instead costs more than the scene itself.
    uint32_t driver_repeat = 1;
    // Empty means every scene the driver mode selects. Otherwise a comma-separated list of scene
    // names without their "scene_" prefix; naming a scene plays it whatever its mode says.
    String driver_scenes;
    driver_mode driver = driver_mode::off;
    bool vsync = true;
    bool resizable          : 1 = true,
         fullscreen         : 1 = false,
         fullscreen_desktop : 1 = false,
         borderless         : 1 = false,
         maximized          : 1 = false;
};

} // namespace floormat
