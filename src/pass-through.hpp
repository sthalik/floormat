#pragma once

#include "pass-mode.hpp"

namespace floormat {

bool can_walk_through(pass_mode p);
bool can_see_through(pass_mode p);
bool can_shoot_through(pass_mode p);

} // namespace floormat
