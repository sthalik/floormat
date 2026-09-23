#pragma once
#include "compat/defs.hpp"
#include <cr/String.h>
#include <mg/Vector2.h>

namespace floormat {

enum class driver_mode : uint8_t { off,
    all,      // selects the same scenes as coverage
    coverage, // every scene
    profile,  // only the scenes marked profile, i.e. not the editor-only ones
};

struct fm_settings
{
    inline fm_settings() noexcept = default;
    virtual ~fm_settings() noexcept;
    fm_DEPRECATED_COPY(fm_settings);
    fm_DEFAULT_MOVE_(fm_settings);

    String title = "floormat editor"_s;
    String load_game;
    const char* const* argv = nullptr; int argc = 0;
    Magnum::Math::Vector2<int> resolution{1024, 720};
    uint32_t fixed_framerate = 0;   // 0 = feed update() the measured frame time
    driver_mode driver = driver_mode::off;
    // Safe because driver waits are counted in frames, never in wall-clock.
    bool driver_no_swapbuffers = false;
    // The two above are read outside the driver too. Guarding drawEvent()'s read would
    // cost it its PGO profile.
#ifndef FLOORMAT_NO_PGO_DRIVER
    // Passes over the scene table. Sampling by restart instead costs more than the scene itself.
    uint32_t driver_repeat = 1;
    // Comma-separated scene names without their "scene_" prefix; naming a scene plays it whatever
    // its mode says. Empty with driver_scenes_given means --driver-scenes=none, i.e. run nothing;
    // empty without it means the driver mode picks.
    String driver_scenes;
    bool driver_scenes_given = false;
    bool driver_save_world = false;
#endif
    bool vsync = true;
    bool resizable          : 1 = true,
         fullscreen         : 1 = false,
         fullscreen_desktop : 1 = false,
         borderless         : 1 = false,
         maximized          : 1 = false,
         minimized          : 1 = false;
};

} // namespace floormat
