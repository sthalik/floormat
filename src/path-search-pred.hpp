#pragma once
#include "compat/function2.fwd.hpp"
#include "collision.hpp"

namespace floormat {

enum class path_search_continue : bool { pass = false, blocked = true };

} // namespace floormat

namespace floormat::detail_astar {

using pred = fu2::function_view<path_search_continue(collision_data) const>;

const pred& never_continue() noexcept;
const pred& always_continue() noexcept;

} // namespace floormat::detail_astar
