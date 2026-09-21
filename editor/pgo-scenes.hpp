#pragma once
#include "compat/array-size.hpp"
#include "editor/app.hpp"
#include "src/grid.hpp"
#include "src/global-coords.hpp"
#include "src/tile-defs.hpp"
#include <cr/StringView.h>

namespace floormat { enum class driver_mode : uint8_t; }

namespace floormat::pgo {

struct scene
{
    StringView name;
    task (app::*fn)();
    // coverage means coverage-only; profile scenes run under both. An editor-only scene marked
    // profile would train the PGO profile on code a shipped build never reaches.
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

// scene_slide's layout, in global tile coords (chunk*TILE_MAX_DIM + local), not chunk-local.
// test/slide.cpp mirrors these. A free SW run holds x+y constant, so the ledge slide
// shifts which diagonal the L sees.
constexpr inline int slide_start_x = 10, slide_start_y = 0;
constexpr inline int slide_ledge_y = 4, slide_ledge_x0 = 5, slide_ledge_x1 = 12;
// y0 reaches north of where the diagonal meets it, so the run cannot clip the top end and miss.
constexpr inline int slide_wall_x = -4, slide_wall_y0 = 8, slide_wall_y1 = 14;
constexpr inline int slide_floor_y = 15, slide_floor_x1 = 2;
// Blocked south by the floor row, west by its own wall_west: both alternatives to SW are gone.
constexpr inline int slide_corner_x = slide_wall_x, slide_corner_y = slide_wall_y1;
// Only these are cleared; the dense scene outside keeps the 8 neighbours sweep_critter() searches
// realistically full.
constexpr inline int16_t slide_chunk_min = -1, slide_chunk_max = 0;
constexpr inline uint8_t slide_bbox = 32;

// Arithmetic shift floors, which is what negative tiles need; a division would truncate toward
// zero and put tile -1 in chunk 0.
constexpr global_coords tile_at(int x, int y)
{
    static_assert(TILE_MAX_DIM == 16);
    return { chunk_coords_{(int16_t)(x >> 4), (int16_t)(y >> 4), 0},
             local_coords{(uint8_t)(x & 15), (uint8_t)(y & 15)} };
}

} // namespace floormat::pgo
