#include "pgo-driver.hpp"
#include "app.hpp"
#include "tests-private.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile.hpp"
#include "src/wall-atlas.hpp"
#include "src/critter.hpp"
#include "src/critter-script-cover.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/raycast.hpp"
#include "src/search-astar.hpp"
#include "src/search-pred.hpp"
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
constexpr local_coords npc_tile{5, 8}, threat_tile{13, 8}, moved_threat_tile{13, 6};
// cover_ai_test's, which its script casts rays with
constexpr uint32_t cover_div = 8;
constexpr uint32_t cycles = 2, short_cycles = 1;
// for each wait on the npc
constexpr auto max_dt = Second*60;

struct stop_s
{
    point pos;
    // from the first update that left the npc at pos
    cover_script_status status;
};

} // namespace

// An npc hides behind a wall from the player, steps out until it sees the player, and goes back.
// cover_ai_test spawns it, lists the other cover spots and resets it. After the player moves, the
// npc peeks from a new spot that sees the player and hides where the player can't see it. Leaving
// the tests mode, switching tests and reset_world() each remove the npc.
task app::scene_cover_ai()
{
    using tests::base_test;
    using tests::Test;
    using state = cover_script_status::state;

    // reset_world() below move-assigns the new world into the one M->world() returns, so w stays
    // valid and then names the new world. No bptr may live across that call, since
    // main_impl::reset_world() asserts that only the world holds its objects.
    auto& w = M->world();
    {
        auto& c = w[chunk_coords_{}];
        maybe_initialize_chunk_(c.coord(), c);
        auto wall = loader.wall_atlas("test1", loader_policy::warn);
        for (auto y = wall_y0; y <= wall_y1; y++)
            c[local_coords{wall_x, y}].wall_west() = { wall, (variant_t)-1 };
        c.mark_modified();
    }

    const auto teleport_player = [&](local_coords tile) {
        auto player = ensure_player_character(w);
        auto index = player->index();
        player->teleport_to(index, point{chunk_coords_{}, tile, {}}, rotation_COUNT);
    };
    teleport_player(threat_tile);

    const auto press = [&](uint32_t button) {
        base_test::Value v;
        v.type = base_test::ValueType::u32;
        v.u32 = button;
        return tests().current_test->advance(*this, v);
    };
    const auto spawn_npc = [&](uint32_t button) {
        const auto v = press(button);
        fm_assert(v.type == base_test::ValueType::u64);
        fm_assert(w.find_object(v.u64));
        return object_id{v.u64};
    };

    do_key(key_mode_tests);
    tests().switch_to(Test::cover_ai);
    auto npc_id = spawn_npc(0);
    const auto start = point{chunk_coords_{}, npc_tile, {}};
    {
        // before the script's first update
        auto npc = w.find_object<critter>(npc_id);
        auto index = npc->index();
        npc->teleport_to(index, start, rotation_COUNT);
    }
    // update_world() only updates objects on screen.
    center_camera_on(point{chunk_coords_{}, local_coords{wall_x, (uint8_t)(wall_y0 + 1)}, {}});
    M->reset_fps();
    co_yield {};

    const auto status = [&] {
        auto C = w.find_object<critter>(npc_id);
        fm_assert(C);
        const auto s = get_cover_script_status(*C);
        fm_assert(s);
        return *s;
    };

    // Each stop is 2 s, so a position held for a second is a stop rather than a walk's pause.
    const auto n_stops = 2*(pgo_driver_use_short_scenes() ? short_cycles : cycles) + 1;
    Array<stop_s> stops;
    stop_s cur{start, status()};
    auto still_since = _driver->scene_dt;
    const auto track = [&] {
        auto C = w.find_object<critter>(npc_id);
        fm_assert(C);
        const auto pos = C->position();
        if (pos == cur.pos)
            return;
        if (_driver->scene_dt - still_since >= Second)
            arrayAppend(stops, cur);
        cur = {pos, status()};
        still_since = _driver->scene_dt;
    };
    const auto check_alternating = [&] {
        for (auto i = 2u; i < stops.size(); i++)
            fm_assert_equal(stops[i - 2].pos, stops[i].pos);
        fm_assert(stops[0].pos != stops[1].pos);
    };

    auto deadline = _driver->scene_dt + max_dt;
    while (stops.size() < n_stops)
    {
        fm_assert(_driver->scene_dt < deadline);
        co_yield {};
        track();
    }
    // stops alternate between the cover spot and the peek spot
    check_alternating();
    fm_assert_equal(stops[0].pos, status().cover);
    const auto cover = Vector3i(stops[0].pos).xy(), peek = Vector3i(stops[1].pos).xy();

    {
        const auto v = press(2);
        fm_assert(v.type == base_test::ValueType::u32);
        fm_assert(v.u32 >= 1);
        Array<point> spots;
        list_cover_spots(*w.find_object<critter>(npc_id), spots);
        fm_assert_equal(v.u32, (uint32_t)spots.size());
        fm_assert_equal(status().cover, spots[0]);
    }

    // The player moves north while the npc peeks. From there the wall hides the gun as well as the
    // cover, so check_exposed finds the ray to the gun blocked, and the npc walks to a new peek spot
    // at once.
    deadline = _driver->scene_dt + max_dt;
    while (status().st != state::exposed)
    {
        fm_assert(_driver->scene_dt < deadline);
        co_yield {};
    }
    teleport_player(moved_threat_tile);
    const auto threat = point{chunk_coords_{}, moved_threat_tile, {}};
    arrayClear(stops);
    cur = {w.find_object(npc_id)->position(), status()};
    still_since = _driver->scene_dt;
    deadline = _driver->scene_dt + max_dt;
    while (stops.size() < n_stops)
    {
        fm_assert(_driver->scene_dt < deadline);
        co_yield {};
        track();
    }
    check_alternating();
    {
        // what the script's sees() casts
        auto& pool = M->astar().pool_registry().pass_pool(Pass::Params{cover_div, pass_mode::shoot_through});
        const auto sees = [&](point to) {
            return raycast(w, threat, to, npc_id, pool, Search::without_critters()).success;
        };
        const auto& p = stops[0];
        fm_assert(p.status.st == state::exposed && p.status.has_peek);
        fm_assert_equal(p.pos, p.status.peek);
        fm_assert_equal(threat, p.status.gun_seen_from);
        fm_assert(sees(p.status.gun));
        const auto& h = stops[1];
        fm_assert(h.status.st == state::hidden);
        fm_assert(!sees(h.pos));
    }
    const auto cover2 = Vector3i(stops[1].pos).xy(), peek2 = Vector3i(stops[0].pos).xy();

    {
        const auto old_id = npc_id;
        npc_id = spawn_npc(3);
        fm_assert(npc_id != old_id);
        fm_assert(!w.find_object(old_id));
    }
    co_yield {};

    do_key(key_mode_none);
    fm_assert(!w.find_object(npc_id));
    co_yield {};

    do_key(key_mode_tests);
    npc_id = spawn_npc(0);
    tests().switch_to(Test::none);
    fm_assert(!w.find_object(npc_id));
    co_yield {};

    // ~cover_ai_test runs after finish_scripts().
    tests().switch_to(Test::cover_ai);
    npc_id = spawn_npc(0);
    reset_world();
    fm_assert(!w.find_object(npc_id));
    co_yield {};

    fm_debug("cover-ai: cover (%d, %d), peek (%d, %d); after the player moved, cover (%d, %d), peek (%d, %d); "
             "%u ms", cover.x(), cover.y(), peek.x(), peek.y(), cover2.x(), cover2.y(), peek2.x(), peek2.y(),
             (uint32_t)Time::to_milliseconds(_driver->scene_dt));

    tests().switch_to(Test::none);
    do_key(key_mode_none);
    co_yield {};
}

} // namespace floormat

#endif
