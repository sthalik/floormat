#pragma once
#include "compat/function2.fwd.hpp"
#include "collision.hpp"

namespace floormat {

class chunk;
struct point;

enum class path_search_continue : bool { pass = false, blocked = true };

} // namespace floormat

namespace floormat::Search {

using heuristic = fu2::function_view<uint32_t(point cur, point goal) const>;
template<typename Chunk = chunk> using Pred = fu2::function_view<path_search_continue(Chunk&,collision_data,Range2D)const>;
using pred = Pred<chunk>;

template<typename Chunk = chunk> const Pred<Chunk>& never_continue() noexcept;
template<typename Chunk = chunk> const Pred<Chunk>& always_continue() noexcept;
template<typename Chunk = chunk> const Pred<Chunk>& without_critters() noexcept;
const heuristic& octile_distance() noexcept;

} // namespace floormat::Search
