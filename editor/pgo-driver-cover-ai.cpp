#include "pgo-driver.hpp"
#include "app.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile.hpp"
#include "src/wall-atlas.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/search-astar.hpp"
#include "src/point.inl"
#include "src/nanosecond.inl"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "floormat/main.hpp"
#include <cr/GrowableArray.h>

#ifndef FLOORMAT_NO_PGO_DRIVER

namespace floormat {

using pgo::task;

namespace {

// west walls of tiles (8, 7) to (8, 9), between the npc and the player
constexpr uint8_t wall_x = 8, wall_y0 = 7, wall_y1 = 9;
constexpr local_coords npc_tile{5, 8}, threat_tile{13, 8};
constexpr uint32_t cover_div = 8;
constexpr uint32_t cycles = 2, short_cycles = 1;
constexpr auto max_dt = Second*60;

} // namespace

// An npc hides behind a wall from the player, steps out until it sees the player, and goes back.
task app::scene_cover_ai()
{
    auto& w = M->world();
    auto& c = w[chunk_coords_{}];
    maybe_initialize_chunk_(c.coord(), c);
    auto wall = loader.wall_atlas("test1", loader_policy::warn);
    for (auto y = wall_y0; y <= wall_y1; y++)
        c[local_coords{wall_x, y}].wall_west() = { wall, (variant_t)-1 };
    c.mark_modified();

    auto player = ensure_player_character(w);
    auto index = player->index();
    player->teleport_to(index, point{chunk_coords_{}, threat_tile, {}}, rotation_COUNT);
    const auto threat_id = player->id;

    critter_proto p;
    p.name = "npc"_s;
    const auto start = point{chunk_coords_{}, npc_tile, {}};
    p.offset = start.offset();
    auto npc = w.make_object<critter>(w.make_id(), start.coord(), move(p));
    const auto npc_id = npc->id;
    npc->script.do_reassign(critter_script::make_cover_script(M->astar(), threat_id, cover_div), npc);
    // update_world() only updates objects on screen.
    center_camera_on(point{chunk_coords_{}, local_coords{wall_x, (uint8_t)(wall_y0 + 1)}, {}});
    M->reset_fps();
    co_yield {};

    // Each stop is 2 s, so a position held for a second is a stop rather than a walk's pause.
    const auto n_stops = 2*(pgo_driver_use_short_scenes() ? short_cycles : cycles) + 1;
    Array<point> stops;
    auto last = start;
    auto still_since = _driver->scene_dt;
    while (stops.size() < n_stops)
    {
        fm_assert(_driver->scene_dt < max_dt);
        co_yield {};
        auto C = w.find_object<critter>(npc_id);
        fm_assert(C);
        const auto pos = C->position();
        if (pos != last)
        {
            if (_driver->scene_dt - still_since >= Second)
                arrayAppend(stops, last);
            last = pos;
            still_since = _driver->scene_dt;
        }
    }

    // stops alternate between the cover spot and the peek spot
    for (auto i = 2u; i < stops.size(); i++)
        fm_assert_equal(stops[i - 2], stops[i]);
    fm_assert(stops[0] != stops[1]);
    const auto cover = Vector3i(stops[0]).xy(), peek = Vector3i(stops[1]).xy();
    fm_debug("cover-ai: cover (%d, %d), peek (%d, %d), %u stops in %u ms", cover.x(), cover.y(), peek.x(), peek.y(),
             (uint32_t)stops.size(), (uint32_t)Time::to_milliseconds(_driver->scene_dt));
}

} // namespace floormat

#endif
