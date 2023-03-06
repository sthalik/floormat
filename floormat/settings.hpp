#pragma once
#include "compat/defs.hpp"
#include <cstdint>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

enum class fm_gpu_debug : char { no_error = 1, off, on, robust, };

struct fm_settings
{
    inline fm_settings() noexcept = default;
    virtual ~fm_settings() noexcept;
    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(fm_settings);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(fm_settings);

    String title = "floormat editor"_s;
    const char* const* argv = nullptr; int argc = 0;
    Magnum::Math::Vector2<int> resolution{1024, 720};
    bool vsync = true;
    bool resizable          : 1 = true,
         fullscreen         : 1 = false,
         fullscreen_desktop : 1 = false,
         borderless         : 1 = false,
         maximized          : 1 = false;
};

} // namespace floormat
