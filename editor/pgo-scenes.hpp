#pragma once
#include "compat/array-size.hpp"
#include "floormat/settings.hpp"
#include "editor/app.hpp"
#include "src/grid.hpp"

namespace floormat::pgo {

struct scene
{
    StringView name;
    task (app::*fn)();
    driver_mode mode;
};

// scene_raycast's sweep, longest first: a quarter-chunk sweep is a tight scribble round the
// character and reads badly as the opening one. The pin field in editor/world-bench.cpp has to
// cover the longest of these and nothing else ties the two files together.
constexpr inline int raycast_radii[] = { 5*(int)chunk_size_xy/4, (int)chunk_size_xy,
                                         3*(int)chunk_size_xy/4, (int)chunk_size_xy/2,
                                         (int)chunk_size_xy/4 };
// Folded rather than indexed, so reordering the sweep cannot silently shrink the pin field.
constexpr inline int raycast_radius_max = []
{
    int m = 0;
    for (const int r : raycast_radii)
        m = r > m ? r : m;
    return m;
}();

// scene_walk's corridor, shared with the generator that carves it and the menu entry that loads
// it. Each side wall eats one interior column, so the passable ones are walk_corridor_tile+1
// through walk_corridor_tile+walk_corridor_width-2.
constexpr inline int16_t walk_chunk_min = -5, walk_chunk_max = 4;
constexpr inline uint8_t walk_corridor_tile = 1, walk_corridor_width = 9;

} // namespace floormat::pgo
