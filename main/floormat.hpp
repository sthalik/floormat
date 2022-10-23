#pragma once
#include <cstdint>
#include <Corrade/Containers/String.h>
#include <Magnum/Math/Vector2.h>

namespace floormat {

enum class fm_gpu_debug : char { no_error = -1, on, off };
enum class fm_tristate : char { maybe = -1, on, off };
enum class fm_log_level : unsigned char { quiet, normal, verbose, };

struct fm_options final
{
    Magnum::Math::Vector2<int> resolution{1024, 768};
    Containers::String title{"Test"};
    Containers::String disabled_extensions; // TODO
    std::uint8_t msaa_samples = 4;
    fm_tristate vsync = fm_tristate::maybe;
    fm_gpu_debug gpu_debug = fm_gpu_debug::on; // TODO
    fm_log_level log_level = fm_log_level::normal; // TODO
    std::uint8_t resizable          : 1 = true,
                 fullscreen         : 1 = false,
                 fullscreen_desktop : 1 = false,
                 borderless         : 1 = false,
                 maximized          : 1 = false,
                 msaa               : 1 = true; // TODO
};

} // namespace floormat

