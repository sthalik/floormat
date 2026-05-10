#pragma once
#include "compat/function2.fwd.hpp"
#include "collision.hpp"

namespace floormat {

class chunk;
struct point;

enum class path_search_continue : bool { pass = false, blocked = true };

} // namespace floormat

namespace floormat::Search {

using pred = fu2::function_view<path_search_continue(chunk& self, collision_data data, Range2D rect) const>;
using const_pred = fu2::function_view<path_search_continue(const chunk& self, collision_data data, Range2D rect) const>;

const pred& never_continue() noexcept;
const pred& always_continue() noexcept;
const const_pred& const_never_continue() noexcept;
const const_pred& const_always_continue() noexcept;

using heuristic = fu2::function_view<uint32_t(point cur, point goal) const>;

#if 0
const heuristic& euclidean_distance() noexcept;
const heuristic& manhattan_distance() noexcept;
#endif

const heuristic& octile_distance() noexcept;

} // namespace floormat::Search
