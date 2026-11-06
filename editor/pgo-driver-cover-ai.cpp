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
#include "src/grid-registry.hpp"
#include "src/raycast.hpp"
#include "src/search-astar.hpp"
#include "src/search-pred.hpp"
#include "src/point.inl"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/nanosecond.inl"
#include "compat/tie.hpp"
#include "floormat/main.hpp"
#include "floormat/settings.hpp"
#include <cr/GrowableArray.h>
#include <cr/StructuredBindings.h>
#include <mg/BatchFunctions.h>

#ifndef FLOORMAT_NO_PGO_DRIVER

namespace floormat {

using pgo::task;

namespace {

// west walls of tiles (8, 7) to (8, 9), between the npc and the player
constexpr u8 wall_x = 8, wall_y0 = 7, wall_y1 = 9;
constexpr local_coords npc_tile{10, 8}, threat_tile{2, 8}, moved_threat_tile{2, 12};
// cover_ai_test's default, which its script casts rays with until the scene picks div 16
constexpr u32 cover_div = Cover::Script::options{}.div_size;
constexpr u32 cycles = 2, short_cycles = 1;
// for each wait on the npc
constexpr auto max_dt = Second*60;

struct stop_s
{
    point pos;
    // from the first update that left the npc at pos
    Cover::Script::status status;
};

// scene_cover_ai_map's walls: four straight ones and two Ls. None encloses a spot, which no path
// would reach.
constexpr local_coords map_west_walls[] = {
    {5, 5}, {5, 6}, {11, 7}, {11, 8}, {9, 10}, {9, 11}, {8, 7},
};
constexpr local_coords map_north_walls[] = {
    {9, 4}, {10, 4}, {11, 9}, {12, 9}, {4, 10}, {5, 10}, {6, 10}, {8, 7},
};
constexpr local_coords map_npc_tile{7, 8};
// Where the player can stand, around the walls and between them. Each move goes to the next one
// from which the player sees the npc.
constexpr local_coords map_player_tiles[] = {
    {14, 5}, {6, 6}, {8, 14}, {9, 8}, {2, 8}, {6, 12}, {8, 2}, {8, 9}, {13, 13}, {3, 9},
    {2, 13}, {10, 6}, {2, 2}, {6, 8}, {13, 2}, {7, 12}, {12, 11}, {4, 3}, {3, 12},
};
// Moves 11 on repeat moves 3 to 10.
constexpr u32 map_moves = 10, short_map_moves = 3;
// A player tile nearer the npc than this is skipped, since the player's body could stand in the
// npc's way.
constexpr u32 map_min_player_dist = 2*tile_size_xy;

// What cover_ai_test's "Other cover spots" box lists once its search has reached every tile: the
// spots of one search over its default radius, 1024 px, around the tile c stands on and from
// threat. Its default octants and max_cover_end_dist are the script's.
void overlay_spots(critter& c, point threat, Array<Cover::Point::spot>& out,
                   u32 max_cover_end_dist = Cover::Script::options{}.listed_cover_end_dist())
{
    const auto s = Cover::Script::get_status(c);
    fm_assert(s && s->has_query);
    auto q = s->query;
    q.pos = point{c.position().coord(), {}};
    q.threat = threat;
    q.radius = 16*tile_size_xy;
    q.max_cover_end_dist = max_cover_end_dist;
    Cover::Script::list_spots(c, q, out);
}

// A peek spot that n spots of a list peek from: nearest, the one nearest it unless tie says two
// are as near, and other, another of them.
struct shared_peek
{
    point peek, nearest, other;
    u32 n = 0;
    bool tie = false;
};

Array<shared_peek> shared_peeks(critter& c, const Array<Cover::Point::spot>& spots)
{
    Array<shared_peek> out;
    arrayReserve(out, spots.size());
    for (const auto& x : spots)
    {
        const auto p = Cover::Script::find_peek(c, x);
        if (!p)
            continue;
        auto i = 0u;
        while (i < out.size() && out[i].peek != p->pos)
            i++;
        if (i == out.size())
        {
            arrayAppend(out, shared_peek{p->pos, x.pos, x.pos, 1});
            continue;
        }
        auto& g = out[i];
        g.n++;
        const auto d = point::distance_squared(x.pos, g.peek), d0 = point::distance_squared(g.nearest, g.peek);
        if (d < d0)
        {
            g.other = g.nearest;
            g.nearest = x.pos;
            g.tie = false;
        }
        else
        {
            g.other = x.pos;
            g.tie |= d == d0;
        }
    }
    return out;
}

bool listed(const Array<Cover::Point::spot>& spots, point pt)
{
    for (const auto& x : spots)
        if (x.pos == pt)
            return true;
    return false;
}

} // namespace

// An npc hides behind a wall from the player, steps out until it sees the player, and goes back.
// cover_ai_test spawns it, lists the other cover spots, sends it to them by its arrows and by
// clicks on their dots, and resets it. Leaving the tests mode, switching tests and reset_world()
// each remove the npc.
task app::scene_cover_ai()
{
    using tests::base_test;
    using tests::Test;
    using state = Cover::Script::status::state;
    using hold_phase = Cover::Script::options::hold_phase;

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

    const auto press = [&](u32 button) {
        base_test::Value v;
        v.type = base_test::ValueType::u32;
        v.u32 = button;
        return tests().current_test->advance(*this, v);
    };
    const auto spawn_npc = [&](u32 button) {
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
    center_camera_on(point{chunk_coords_{}, local_coords{wall_x, (u8)(wall_y0 + 1)}, {}});
    M->reset_fps();
    co_yield {};

    const auto status = [&] {
        auto C = w.find_object<critter>(npc_id);
        fm_assert(C);
        const auto s = Cover::Script::get_status(*C);
        fm_assert(s);
        return *s;
    };

    // Each stop is 2 s, so a position held for a second is a stop rather than a walk's pause.
    const auto n_stops = 2*(pgo_driver_use_short_scenes() ? short_cycles : cycles) + 1;
    Array<stop_s> stops;
    arrayReserve(stops, n_stops);
    stop_s cur{start, status()};
    auto still_since = M->world().game_time();
    const auto track = [&] {
        auto C = w.find_object<critter>(npc_id);
        fm_assert(C);
        const auto pos = C->position();
        if (pos == cur.pos)
            return;
        if (M->world().game_time() - still_since >= Second)
            arrayAppend(stops, cur);
        cur = {pos, status()};
        still_since = M->world().game_time();
    };
    const auto check_alternating = [&] {
        for (auto i = 2u; i < stops.size(); i++)
            fm_assert_equal(stops[i - 2].pos, stops[i].pos);
        fm_assert(stops[0].pos != stops[1].pos);
    };

    auto deadline = M->world().game_time() + max_dt;
    while (stops.size() < n_stops)
    {
        fm_assert(M->world().game_time() < deadline);
        co_yield {};
        track();
    }
    // stops alternate between the cover spot and the peek spot
    check_alternating();
    fm_assert_equal(stops[0].pos, status().cover);
    const auto cover = Vector3i(stops[0].pos).xy(), peek = Vector3i(stops[1].pos).xy();

    Array<Cover::Point::spot> overlay;
    {
        const auto v = press(2);
        fm_assert(v.type == base_test::ValueType::u32);
        fm_assert(v.u32 >= 1);
        overlay_spots(*w.find_object<critter>(npc_id), ensure_player_character(w)->position(), overlay);
        fm_assert_equal(v.u32, (u32)overlay.size());
        fm_assert(listed(overlay, status().canonical));
    }

    // The next arrow beside the cover combobox, then clicks on peek spots' dots, each followed by a
    // click on the first listed spot's dot, then the next arrow and the previous one. After each,
    // the npc goes on as from cover it found itself. With threat_reach, the search lists spots where
    // place() put them, so spots along one face share a peek spot.
    {
        const auto npc_at = [&] { return w.find_object(npc_id)->position(); };
        const auto hidden_at = [&](point p) {
            const auto s = status();
            return s.st == state::hidden && s.canonical == p && npc_at() == s.cover;
        };
        const auto exposed_at = [&](point p) {
            const auto s = status();
            return s.st == state::exposed && s.has_peek && s.peek == p && npc_at() == p;
        };
        const auto first = status().canonical;
        deadline = M->world().game_time() + max_dt;
        while (!hidden_at(first))
        {
            fm_assert(M->world().game_time() < deadline);
            co_yield {};
        }

        const auto v = press(4);
        fm_assert(v.type == base_test::ValueType::u32);
        Array<Cover::Point::spot> spots;
        Cover::Script::list_spots(*w.find_object<critter>(npc_id), spots);
        fm_assert(v.u32 > 0 && v.u32 < spots.size());
        const auto switched = spots[v.u32];
        fm_assert_equal(switched.pos, status().canonical);
        const auto p = Cover::Script::find_peek(*w.find_object<critter>(npc_id), switched);
        fm_assert(p);
        // hidden, out and back, as at cover it found itself
        for (const auto exposed : { false, true, false })
        {
            deadline = M->world().game_time() + max_dt;
            while (!(exposed ? exposed_at(p->pos) : hidden_at(switched.pos)))
            {
                fm_assert(M->world().game_time() < deadline);
                co_yield {};
            }
        }

        // The box's spots are still those of the search above, since the player hasn't moved.
        fm_assert(listed(overlay, switched.pos));
        struct target { point peek, cover; };
        target targets[2] = { {p->pos, switched.pos}, };
        auto n_targets = 1u;
        Optional<Cover::Point::spot> nearest;
        point other_peek;
        auto first_shares = false;
        for (const auto& x : overlay)
        {
            const auto q = Cover::Script::find_peek(*w.find_object<critter>(npc_id), x);
            if (!q || q->pos == p->pos || nearest && q->pos != other_peek)
                continue;
            if (!nearest || point::distance_squared(x.pos, q->pos) < point::distance_squared(nearest->pos, q->pos))
                nearest = x;
            other_peek = q->pos;
            first_shares |= x.pos == first;
        }
        // The npc hides at first at the click, so the panel peeks from first if first shares the peek spot.
        if (nearest)
            targets[n_targets++] = {other_peek, first_shares ? first : nearest->pos};
        for (auto t = 0u; t < n_targets; t++)
        {
            const auto [peek, cover] = targets[t];
            set_cursor_at(peek);
            co_await click_at_cursor(mouse_button_left);
            {
                const auto s = status();
                fm_assert(s.st == state::to_peek);
                fm_assert(s.canonical == cover && s.has_peek && s.peek == peek);
            }
            deadline = M->world().game_time() + max_dt;
            while (!exposed_at(peek))
            {
                fm_assert(M->world().game_time() < deadline);
                co_yield {};
            }
            deadline = M->world().game_time() + max_dt;
            while (!hidden_at(cover))
            {
                fm_assert(M->world().game_time() < deadline);
                co_yield {};
            }
            if (cover == first)
                continue;
            set_cursor_at(first);
            co_await click_at_cursor(mouse_button_left);
            fm_assert(status().st == state::to_cover);
            fm_assert_equal(first, status().canonical);
            deadline = M->world().game_time() + max_dt;
            while (!hidden_at(first))
            {
                fm_assert(M->world().game_time() < deadline);
                co_yield {};
            }
        }

        // From the same cover, with nothing moved, the next arrow picks the same spot, and the
        // previous one goes back past the spots the next one skipped.
        const auto next = press(4);
        fm_assert(next.type == base_test::ValueType::u32);
        fm_assert_equal(v.u32, next.u32);
        fm_assert_equal(switched.pos, status().canonical);
        const auto prev = press(5);
        fm_assert(prev.type == base_test::ValueType::u32);
        fm_assert_equal(first, spots[prev.u32].pos);
        fm_assert_equal(first, status().canonical);
        deadline = M->world().game_time() + max_dt;
        while (!hidden_at(first))
        {
            fm_assert(M->world().game_time() < deadline);
            co_yield {};
        }

        // The Hold box, ticked while the npc hides at first, holds hidden, and a click on first's
        // peek spot then holds exposed: the npc stays out 4 s, twice its time out. Unticking "Move
        // spots to cover's end" applies the panel's options while it is out, and it stays out,
        // since the click moved the panel's hold too.
        {
            const auto h = press(6);
            fm_assert(h.type == base_test::ValueType::u32);
            fm_assert_equal((u32)hold_phase::hidden, h.u32);
        }
        const auto q = Cover::Script::find_peek(*w.find_object<critter>(npc_id), spots[prev.u32]);
        fm_assert(q);
        set_cursor_at(q->pos);
        co_await click_at_cursor(mouse_button_left);
        fm_assert(status().st == state::to_peek);
        deadline = M->world().game_time() + max_dt;
        while (!exposed_at(q->pos))
        {
            fm_assert(M->world().game_time() < deadline);
            co_yield {};
        }
        for (const auto until = M->world().game_time() + Second*4; M->world().game_time() < until;)
        {
            co_yield {};
            fm_assert(exposed_at(q->pos));
        }
        {
            const auto e = press(7);
            fm_assert(e.type == base_test::ValueType::u32);
            fm_assert_equal(0u, e.u32);
        }
        for (const auto until = M->world().game_time() + Second; M->world().game_time() < until;)
        {
            co_yield {};
            fm_assert(exposed_at(q->pos));
        }

        // Spots left where the search accepts them peek from the same spot past each end of the
        // wall. Two peek spots shared with no tie for the nearest: the npc first hides at a spot of
        // the first other than the nearest, held hidden for 3 s, then peeks from there, then from
        // the nearest spot of the second.
        Array<Cover::Point::spot> unmoved;
        {
            const auto v = press(2);
            fm_assert(v.type == base_test::ValueType::u32);
            overlay_spots(*w.find_object<critter>(npc_id), ensure_player_character(w)->position(), unmoved, 0);
            fm_assert_equal(v.u32, (u32)unmoved.size());
        }
        const auto shared = shared_peeks(*w.find_object<critter>(npc_id), unmoved);
        const shared_peek* own_peek = nullptr;
        const shared_peek* near_peek = nullptr;
        for (const auto& g : shared)
            if (g.n < 2 || g.tie)
                continue;
            else if (!own_peek)
                own_peek = &g;
            else if (!near_peek)
                near_peek = &g;
        fm_assert(own_peek && near_peek);
        set_cursor_at(own_peek->other);
        co_await click_at_cursor(mouse_button_left);
        fm_assert(status().st == state::to_cover);
        deadline = M->world().game_time() + max_dt;
        while (!hidden_at(own_peek->other))
        {
            fm_assert(M->world().game_time() < deadline);
            co_yield {};
        }
        for (const auto until = M->world().game_time() + Second*3; M->world().game_time() < until;)
        {
            co_yield {};
            fm_assert(hidden_at(own_peek->other));
        }
        const target peek_clicks[] = { {own_peek->peek, own_peek->other}, {near_peek->peek, near_peek->nearest} };
        for (const auto [peek, cover] : peek_clicks)
        {
            set_cursor_at(peek);
            co_await click_at_cursor(mouse_button_left);
            {
                const auto s = status();
                fm_assert(s.st == state::to_peek);
                fm_assert(s.canonical == cover && s.has_peek && s.peek == peek);
            }
            deadline = M->world().game_time() + max_dt;
            while (!exposed_at(peek))
            {
                fm_assert(M->world().game_time() < deadline);
                co_yield {};
            }
        }

        {
            const auto e = press(7);
            fm_assert(e.type == base_test::ValueType::u32);
            fm_assert_equal(1u, e.u32);
            const auto v = press(2);
            fm_assert(v.type == base_test::ValueType::u32);
            fm_assert_equal((u32)overlay.size(), v.u32);
        }
        set_cursor_at(first);
        co_await click_at_cursor(mouse_button_left);
        fm_assert(status().st == state::to_cover);
        deadline = M->world().game_time() + max_dt;
        while (!hidden_at(first))
        {
            fm_assert(M->world().game_time() < deadline);
            co_yield {};
        }
        {
            const auto h = press(6);
            fm_assert(h.type == base_test::ValueType::u32);
            fm_assert_equal((u32)hold_phase::none, h.u32);
        }
        fm_debug("cover-ai: switched to spot %u of %u, %u peek spots clicked; %u spots left where accepted, "
                 "peek spots shared by %u and %u", v.u32, (u32)spots.size(), n_targets, (u32)unmoved.size(),
                 own_peek->n, near_peek->n);
    }

    // The player moves south while the npc peeks. From there the wall hides the gun as well as the
    // cover, so check_exposed finds the ray to the gun blocked, and the npc walks to a new peek spot
    // at once.
    deadline = M->world().game_time() + max_dt;
    while (status().st != state::exposed)
    {
        fm_assert(M->world().game_time() < deadline);
        co_yield {};
    }
    teleport_player(moved_threat_tile);
    const auto threat = point{chunk_coords_{}, moved_threat_tile, {}};
    arrayClear(stops);
    cur = {w.find_object(npc_id)->position(), status()};
    still_since = M->world().game_time();
    deadline = M->world().game_time() + max_dt;
    while (stops.size() < n_stops)
    {
        fm_assert(M->world().game_time() < deadline);
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
        fm_assert_equal(threat, p.status.gun_ray_from);
        fm_assert(sees(p.status.gun));
        const auto& h = stops[1];
        fm_assert(h.status.st == state::hidden);
        fm_assert(!sees(h.pos));
    }
    const auto cover2 = Vector3i(stops[1].pos).xy(), peek2 = Vector3i(stops[0].pos).xy();

    // A new div starts the box's search over, in grids no search has filled. It lists up to 32 tiles,
    // cover_ai_test's spots_tiles_per_frame, with at most one octant fill per frame.
    u32 div_frames = 0, div_fills = 0;
    {
        base_test::Value v;
        v.type = base_test::ValueType::au32;
        v.au32[0] = 8;
        v.au32[1] = 16;
        v = tests().current_test->advance(*this, v);
        fm_assert(v.type == base_test::ValueType::u32);
        fm_assert_equal(16u, v.u32);
        u32 tiles = 0;
        for (;;)
        {
            co_yield {};
            div_frames++;
            const auto p = press(9);
            fm_assert(p.type == base_test::ValueType::au32 && p.au32[3]);
            fm_assert(p.au32[0] >= tiles && p.au32[0] - tiles <= 32);
            fm_assert(p.au32[2] >= div_fills && p.au32[2] - div_fills <= 1);
            fm_assert(div_frames > 1 || p.au32[0] < p.au32[1]);
            tiles = p.au32[0];
            div_fills = p.au32[2];
            if (tiles == p.au32[1])
                break;
            fm_assert(div_frames < 10000);
        }
        fm_assert(div_fills > 1);
    }

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
             "div 16 spots in %u frames, %u octant fills; %u ms", cover.x(), cover.y(), peek.x(), peek.y(),
             cover2.x(), cover2.y(), peek2.x(), peek2.y(), div_frames, div_fills,
             (u32)Time::to_milliseconds(M->world().game_time()));

    tests().switch_to(Test::none);
    do_key(key_mode_none);
    co_yield {};
}

// Walls in six places stand between the npc and the player. The player keeps moving to where it
// sees the npc. After each move, the npc hides at a new spot the player can't see, then peeks from
// a spot from which the player sees its gun, and goes back.
task app::scene_cover_ai_map()
{
    using tests::base_test;
    using tests::Test;
    using state = Cover::Script::status::state;

    auto& w = M->world();
    {
        auto& c = w[chunk_coords_{}];
        maybe_initialize_chunk_(c.coord(), c);
        auto wall = loader.wall_atlas("test1", loader_policy::warn);
        for (const auto tile : map_west_walls)
            c[tile].wall_west() = { wall, (variant_t)-1 };
        for (const auto tile : map_north_walls)
            c[tile].wall_north() = { wall, (variant_t)-1 };
        c.mark_modified();
    }

    const auto tile_pos = [](local_coords tile) { return point{chunk_coords_{}, tile, {}}; };
    u32 at = 0;
    const auto teleport_player = [&](u32 k) {
        at = k;
        auto player = ensure_player_character(w);
        auto index = player->index();
        player->teleport_to(index, tile_pos(map_player_tiles[k]), rotation_COUNT);
    };
    teleport_player(0);

    do_key(key_mode_tests);
    tests().switch_to(Test::cover_ai);
    object_id npc_id;
    {
        base_test::Value v;
        v.type = base_test::ValueType::u32;
        v.u32 = 0;
        v = tests().current_test->advance(*this, v);
        fm_assert(v.type == base_test::ValueType::u64);
        npc_id = v.u64;
        auto npc = w.find_object<critter>(npc_id);
        fm_assert(npc);
        auto index = npc->index();
        npc->teleport_to(index, tile_pos(map_npc_tile), rotation_COUNT);
    }
    center_camera_on(tile_pos({8, 8}));
    M->reset_fps();
    co_yield {};

    const auto npc_pos = [&] {
        auto C = w.find_object(npc_id);
        fm_assert(C);
        return C->position();
    };
    const auto status = [&] {
        auto C = w.find_object<critter>(npc_id);
        fm_assert(C);
        const auto s = Cover::Script::get_status(*C);
        fm_assert(s);
        return *s;
    };
    // what the script's sees() casts
    auto& pool = M->astar().pool_registry().pass_pool(Pass::Params{cover_div, pass_mode::shoot_through});
    const auto sees = [&](point from, point to) {
        return raycast(w, from, to, npc_id, pool, Search::without_critters()).success;
    };
    const auto print = [](point pt) {
        const auto v = Vector3i(pt).xy();
        return std::pair{v.x(), v.y()};
    };

    const auto n_moves = pgo_driver_use_short_scenes() ? short_map_moves : map_moves;
    Array<point> covers;
    arrayReserve(covers, n_moves + 1);
    for (auto k = 0u; ; k++)
    {
        const auto player = tile_pos(map_player_tiles[at]);
        auto deadline = M->world().game_time() + max_dt;
        for (;;)
        {
            if (const auto s = status(); s.st == state::hidden && npc_pos() == s.cover && (!k || s.cover != covers.back()))
                break;
            if (M->world().game_time() >= deadline)
            {
                const auto s = status();
                const auto [x, y] = print(npc_pos());
                const auto [cx, cy] = print(s.cover);
                const auto [tx, ty] = print(player);
                fm_abort("cover-ai-map: move %u, player (%d, %d): the npc doesn't hide at a new spot: "
                         "state %u at (%d, %d), cover (%d, %d), has_cover %d, searches %u",
                         k, tx, ty, (unsigned)s.st, x, y, cx, cy, (int)s.has_cover, s.num_searches);
            }
            co_yield {};
        }
        const auto cover = npc_pos();
        if (sees(player, cover))
        {
            const auto [x, y] = print(cover);
            fm_abort("cover-ai-map: move %u: the player sees cover (%d, %d)", k, x, y);
        }
        arrayAppend(covers, cover);

        u32 n_spots, n_peeks = 0;
        bool peeks = false;
        Ns box_time;
        {
            base_test::Value v;
            v.type = base_test::ValueType::u32;
            v.u32 = 2;
            auto t = Time::now();
            v = tests().current_test->advance(*this, v);
            box_time = t.update();
            fm_assert(v.type == base_test::ValueType::u32);
            n_spots = v.u32;
            auto& C = *w.find_object<critter>(npc_id);
            Array<Cover::Point::spot> spots;
            overlay_spots(C, player, spots);
            fm_assert_equal(n_spots, (u32)spots.size());
            const auto canonical = status().canonical;
            fm_assert(listed(spots, canonical));
            for (const auto& x : spots)
            {
                fm_assert(!sees(player, x.pos));
                const auto peek = !!Cover::Script::find_peek(C, x);
                n_peeks += peek;
                peeks |= peek && x.pos == canonical;
            }
        }

        const auto [cx, cy] = print(cover);
        const auto [tx, ty] = print(player);
        // Without a peek spot, step_out hides the npc again for each hidden_time.
        if (!peeks)
        {
            fm_debug("cover-ai-map: move %u, player (%d, %d): cover (%d, %d) has no peek spot, %u spots listed, "
                     "%u with peek spots, whole search %.0f us", k, tx, ty, cx, cy, n_spots, n_peeks,
                     (double)Time::to_milliseconds(box_time)*1e3);
        }
        else
        {
            deadline = M->world().game_time() + max_dt;
            for (;;)
            {
                if (const auto s = status(); s.st == state::exposed && npc_pos() == s.peek)
                    break;
                if (M->world().game_time() >= deadline)
                    fm_abort("cover-ai-map: move %u: the npc doesn't peek from cover (%d, %d)", k, cx, cy);
                co_yield {};
            }
            const auto s = status();
            fm_assert_equal(player, s.gun_ray_from);
            fm_assert(sees(player, s.gun));
            const auto [px, py] = print(s.peek);
            fm_debug("cover-ai-map: move %u, player (%d, %d): cover (%d, %d), peek (%d, %d), %u spots listed, "
                     "%u with peek spots, whole search %.0f us", k, tx, ty, cx, cy, px, py, n_spots, n_peeks,
                     (double)Time::to_milliseconds(box_time)*1e3);
        }
        if (k == n_moves)
            break;

        deadline = M->world().game_time() + max_dt;
        while (!(status().st == state::hidden && npc_pos() == cover))
        {
            fm_assert(M->world().game_time() < deadline);
            co_yield {};
        }
        auto next = (u32)-1;
        for (auto j = 1u; j < array_size(map_player_tiles); j++)
        {
            const auto i = (at + j) % (u32)array_size(map_player_tiles);
            const auto pt = tile_pos(map_player_tiles[i]);
            if (point::distance(pt, cover) >= map_min_player_dist && sees(pt, cover))
            {
                next = i;
                break;
            }
        }
        if (next == (u32)-1)
        {
            const auto [x, y] = print(cover);
            fm_abort("cover-ai-map: move %u: no player tile sees cover (%d, %d)", k, x, y);
        }
        teleport_player(next);
        co_yield {};
    }

    // The player moves to the nearest place, with its bearing from the box's center in the same octant,
    // that sees a listed spot. The box drops those it sees then, without starting over.
    u32 dropped = 0;
    {
        const auto press = [&](u32 button) {
            base_test::Value v;
            v.type = base_test::ValueType::u32;
            v.u32 = button;
            return tests().current_test->advance(*this, v);
        };
        const auto box = press(11);
        fm_assert(box.type == base_test::ValueType::ai32 && box.ai32[3]);
        const auto center = tile_pos({}) + Vector2i{box.ai32[0], box.ai32[1]};
        const auto octant = (u32)box.ai32[2];
        const auto from = tile_pos(map_player_tiles[at]);
        auto& C = *w.find_object<critter>(npc_id);
        auto q = Cover::Script::get_status(C)->query;
        q.pos = center;
        q.threat = from;
        q.radius = 16*tile_size_xy;
        q.max_cover_end_dist = Cover::Script::options{}.listed_cover_end_dist();
        Array<Cover::Point::spot> listed_spots;
        Cover::Script::list_spots(C, q, listed_spots);
        const auto before = press(10);
        fm_assert_equal(0u, before.au32[1]);
        fm_assert_equal(before.au32[2], (u32)listed_spots.size());
        constexpr i8 offsets[] = { -24, -8, 8, 24, };
        Optional<point> to;
        for (u8 y = 0; y < TILE_MAX_DIM; y++)
            for (u8 x = 0; x < TILE_MAX_DIM; x++)
                for (const auto oy : offsets)
                    for (const auto ox : offsets)
                    {
                        const auto pt = point{chunk_coords_{}, {x, y}, {ox, oy}};
                        if (Cover::Point::nearest_octant(Vector2(pt - center)) != octant ||
                            to && point::distance_squared(pt, from) >= point::distance_squared(*to, from))
                            continue;
                        for (const auto& s : listed_spots)
                            if (sees(pt, s.pos))
                            {
                                to = pt;
                                break;
                            }
                    }
        fm_assert(to);
        auto player = ensure_player_character(w);
        auto index = player->index();
        player->teleport_to(index, *to, rotation_COUNT);
        co_yield {};
        const auto after = press(10);
        fm_assert(after.type == base_test::ValueType::au32);
        fm_assert_equal(0u, after.au32[0]);
        dropped = after.au32[1];
        fm_assert(dropped > 0);
        fm_assert_equal(before.au32[2] - dropped, after.au32[2]);
    }

    auto distinct = 0u;
    for (auto i = 0u; i < covers.size(); i++)
    {
        auto j = 0u;
        while (j < i && covers[j] != covers[i])
            j++;
        distinct += j == i;
    }
    fm_debug("cover-ai-map: %u moves, %u cover spots, %u spots dropped as seen, %u ms", n_moves, distinct, dropped,
             (u32)Time::to_milliseconds(M->world().game_time()));

    tests().switch_to(Test::none);
    do_key(key_mode_none);
    co_yield {};
}

task app::scene_cover_wait()
{
    using tests::base_test;
    using tests::Test;
    using state = Cover::Script::status::state;
    using hold_phase = Cover::Script::options::hold_phase;
    constexpr auto wait_time = 2*Second;
    constexpr u32 min_hz = 60, max_hz = 144, held_updates = 40, held_exposed_updates = 60;

    auto& w = M->world();
    {
        auto& c = w[chunk_coords_{}];
        maybe_initialize_chunk_(c.coord(), c);
        auto wall = loader.wall_atlas("test1", loader_policy::warn);
        for (auto y = wall_y0; y <= wall_y1; y++)
            c[local_coords{wall_x, y}].wall_west() = { wall, (variant_t)-1 };
        c.mark_modified();
    }
    {
        auto player = ensure_player_character(w);
        auto index = player->index();
        player->teleport_to(index, point{chunk_coords_{}, threat_tile, {}}, rotation_COUNT);
    }
    const auto press = [&](u32 button) {
        base_test::Value v;
        v.type = base_test::ValueType::u32;
        v.u32 = button;
        return tests().current_test->advance(*this, v);
    };

    do_key(key_mode_tests);
    tests().switch_to(Test::cover_ai);
    object_id npc_id;
    {
        const auto v = press(0);
        fm_assert(v.type == base_test::ValueType::u64);
        npc_id = object_id{v.u64};
        auto npc = w.find_object<critter>(npc_id);
        fm_assert(npc);
        auto index = npc->index();
        npc->teleport_to(index, point{chunk_coords_{}, npc_tile, {}}, rotation_COUNT);
    }
    center_camera_on(point{chunk_coords_{}, local_coords{wall_x, (u8)(wall_y0 + 1)}, {}});
    co_yield {};

    const auto status = [&] {
        auto C = w.find_object<critter>(npc_id);
        fm_assert(C);
        const auto s = Cover::Script::get_status(*C);
        fm_assert(s);
        return *s;
    };
    const auto name = [](state s) { return s == state::hidden ? "hidden" : "exposed"; };

    struct phase_s
    {
        state st = state::seek;
        Ns spent, wait;
        u32 updates = 0, held = 0;
        bool active = false, paused = false;
    };
    phase_s ph;
    auto& hz = M->settings().fixed_framerate;
    const auto hz0 = hz;
    u32 seed = 0x9e3779b9u, phases = 0, held_left = 0, num_hidden = 0, num_exposed = 0, num_paused = 0;
    u32 exposed_held = 0;
    auto hold = hold_phase::none;
    bool cut = false;
    auto prev_st = status().st;
    auto prev_t = w.game_time();
    auto min_dt = Ns{}, max_dt_seen = Ns{};
    const auto deadline = w.game_time() + max_dt;

    while (exposed_held < held_exposed_updates)
    {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        hz = min_hz + seed % (max_hz - min_hz + 1);
        co_yield {};
        fm_assert(w.game_time() < deadline);
        const auto t = w.game_time(), dt = t - prev_t;
        tie(min_dt, max_dt_seen) = min_dt == Ns{}
            ? pair(dt, dt)
            : Math::minmax({min_dt, max_dt_seen, dt});
        const auto st = status().st;
        const auto held = prev_st == state::hidden && hold == hold_phase::hidden ||
                          prev_st == state::exposed && hold == hold_phase::exposed;
        if (ph.active)
        {
            ph.updates++;
            if (prev_st == ph.st && held)
                ph.held++;
            else if (prev_st == ph.st)
                ph.spent += dt;
            const auto due = ph.spent >= ph.wait;
            if (due != (st != ph.st))
                fm_abort("cover-wait: %s wait ended %s, on update %u of the phase: %llu of %llu ns spent, dt %llu ns",
                         name(ph.st), due ? "late" : "early", ph.updates, (unsigned long long)ph.spent.stamp,
                         (unsigned long long)ph.wait.stamp, (unsigned long long)dt.stamp);
            if (due)
            {
                fm_debug("cover-wait: %s wait ended on update %u of the phase, %u held, %llu of %llu ns spent",
                         name(ph.st), ph.updates, ph.held, (unsigned long long)ph.spent.stamp,
                         (unsigned long long)ph.wait.stamp);
                (ph.st == state::hidden ? num_hidden : num_exposed)++;
                num_paused += ph.held != 0;
                ph.active = false;
            }
        }
        if (st != prev_st && (st == state::hidden || st == state::exposed))
        {
            fm_assert(!ph.active);
            ph = { st, Ns{}, wait_time, 0, 0, true, false };
            phases++;
        }
        if (ph.active && (phases == 3 || phases == 4) && !ph.paused && ph.spent >= wait_time/4u)
        {
            hold = (hold_phase)press(6).u32;
            fm_assert(hold == (ph.st == state::hidden ? hold_phase::hidden : hold_phase::exposed));
            ph.paused = true;
            held_left = held_updates;
        }
        else if (held_left && !--held_left)
        {
            hold = (hold_phase)press(6).u32;
            fm_assert(hold == hold_phase::none);
        }
        if (ph.active && phases == 5 && !cut && ph.spent >= wait_time/3u)
        {
            fm_assert(ph.st == state::hidden);
            auto o = Cover::Script::options{};
            o.hold = hold = hold_phase::exposed;
            Cover::Script::set_options(*w.find_object<critter>(npc_id), o);
            ph.wait = ph.spent;
            cut = true;
        }
        if (phases == 6 && ph.active && ph.updates)
            exposed_held++;
        prev_st = st;
        prev_t = t;
    }
    hz = hz0;

    fm_assert_equal(3u, num_hidden);
    fm_assert_equal(2u, num_exposed);
    fm_assert_equal(2u, num_paused);
    fm_assert(min_dt < max_dt_seen);
    fm_debug("cover-wait: %u hidden and %u exposed waits ended on time, dt %llu to %llu ns, %u ms", num_hidden,
             num_exposed, (unsigned long long)min_dt.stamp, (unsigned long long)max_dt_seen.stamp,
             (u32)Time::to_milliseconds(w.game_time()));

    tests().switch_to(Test::none);
    do_key(key_mode_none);
    co_yield {};
}

} // namespace floormat

#endif
