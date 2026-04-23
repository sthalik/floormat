#pragma once
#include "pass-mode.hpp"
#include <array>

namespace floormat {

using pass_through_mask = std::array<bool, (size_t)pass_mode::COUNT>;

extern const pass_through_mask can_walk_through_mask;
extern const pass_through_mask can_see_through_mask;
extern const pass_through_mask can_shoot_through_mask;
extern const pass_through_mask not_blocked_pass_through_mask;

#if 0
bool can_walk_through(pass_mode p);
bool can_see_through(pass_mode p);
bool can_shoot_through(pass_mode p);
#endif

bool can_pass_through_mask(pass_through_mask mask, pass_mode p);

} // namespace floormat
