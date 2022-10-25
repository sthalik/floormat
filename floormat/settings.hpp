#pragma once
#include "compat/defs.hpp"
#include <cstdint>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

enum class fm_gpu_debug : char { no_error = 1, off, on, robust, };
enum class fm_tristate : char { maybe = -1, on, off };
enum class fm_log_level : unsigned char { quiet, normal, verbose, };

struct fm_settings
{
    inline fm_settings() noexcept = default;
    virtual ~fm_settings() noexcept;
    fm_DECLARE_DEPRECATED_COPY_ASSIGNMENT(fm_settings);
    fm_DECLARE_DEFAULT_MOVE_ASSIGNMENT_(fm_settings);

    Magnum::Math::Vector2<int> resolution{1024, 768};
    Corrade::Containers::String title{"Test"};
    Corrade::Containers::String disabled_extensions; // TODO
    std::uint8_t msaa_samples = 16;
    fm_tristate vsync = fm_tristate::maybe;
    fm_gpu_debug gpu_debug = fm_gpu_debug::on;
    fm_log_level log_level = fm_log_level::normal;
    std::uint8_t resizable          : 1 = true,
                 fullscreen         : 1 = false,
                 fullscreen_desktop : 1 = false,
                 borderless         : 1 = false,
                 maximized          : 1 = false,
                 msaa               : 1 = true;
};

} // namespace floormat
