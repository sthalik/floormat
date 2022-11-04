#pragma once
#include "compat/integer-types.hpp"

namespace floormat {

using scenery_t = std::uint16_t;
using scenery_frame_t = std::uint8_t;

enum class scenery : scenery_t {
    none, door_closed, door_empty,
};

} // namespace floormat
