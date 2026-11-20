#pragma once
#include "point.hpp"
#include "search-pred.hpp"
#include "tile-defs.hpp"
#include "compat/function-view-fwd.hpp"
#include <cr/Optional.h>

namespace floormat { struct critter; }
namespace floormat::Cover::Script { struct peek; }

namespace floormat::Cover {

// peek_spot() looks for a peek spot at most this many px from `from`, the cover end.
constexpr inline int max_peek_spot_dist = 2*tile_size_xy;

// Where the gun is when c's body is centered on pos and c faces threat: offset px to its right, or
// half of c's bbox_size along across without offset. A negative offset puts it on the left.
point gun_point(const critter& c, point pos, point threat, Vector2i across, Optional<int> offset);
// The spot nearest `from`, up to 2 tiles away along ±axis, across it or diagonally, where sees() is true for
// gun_point() of c's body there, and that gun point. A seen stretch shorter than half a body can be missed.
Optional<Cover::Script::peek> peek_spot(critter& c, point from, Vector2i axis, point threat, Optional<int> gun_offset,
                                        const Search::pred& pred, function_view<bool(point)> sees);

} // namespace floormat::Cover
