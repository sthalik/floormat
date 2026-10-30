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
constexpr local_coords npc_tile{10, 8}, threat_tile{2, 8}, moved_threat_tile{2, 12};
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

// scene_cover_ai_map's walls: four straight ones and two Ls. None encloses a spot. A box of walls
// 32 px deep leaves room inside for the npc's body, and the script, which tries only the nearest
// cover, then keeps picking that spot with no path.
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
// After the 8th move, from (2, 2), the npc hides in the corner of the L at (8, 7) and finds no peek
// spot, since sliding along one axis either way runs into the L or stays hidden for max_peek.
constexpr uint32_t map_moves = 7, short_map_moves = 3;
// A player tile nearer the npc than this is skipped, since the player's body could stand in the
// npc's way.
constexpr uint32_t map_min_player_dist = 2*tile_size_xy;

// What cover_ai_test's "Other cover spots" box lists once its search has reached every tile: the
// spots of one search over its default radius, 1024 px, around the tile c stands on and from
// threat. Its default octants are the script's.
void overlay_spots(critter& c, point threat, Array<cover_spot>& out)
{
    const auto s = get_cover_script_status(c);
    fm_assert(s && s->has_query);
    auto q = s->query;
    q.pos = point{c.position().coord(), {}};
    q.threat = threat;
    q.radius = 16*tile_size_xy;
    list_cover_spots(c, q, out);
}

bool listed(const Array<cover_spot>& spots, point pt)
{
    for (const auto& x : spots)
        if (x.pos == pt)
            return true;
    return false;
}

} // namespace

// An npc hides behind a wall from the player, steps out until it sees the player, and goes back.
// cover_ai_test spawns it, lists the other cover spots, sends it to them by its button and by clicks
// on their dots, and resets it. After the player moves, the npc peeks from a new spot that sees
// the player and hides where the player can't see it. Leaving the tests mode, switching tests and
// reset_world() each remove the npc.
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

    Array<cover_spot> overlay;
    {
        const auto v = press(2);
        fm_assert(v.type == base_test::ValueType::u32);
        fm_assert(v.u32 >= 1);
        overlay_spots(*w.find_object<critter>(npc_id), ensure_player_character(w)->position(), overlay);
        fm_assert_equal(v.u32, (uint32_t)overlay.size());
        fm_assert(listed(overlay, status().cover));
    }

    // The "Switch to other cover" button, then clicks on peek spots' dots, then one on the first
    // listed spot's dot, which sends the npc back to its cover. After each, the npc goes on as from
    // cover it found itself. Spots along the wall's face slide to the same peek spots. The first
    // click is on the peek spot of the npc's cover, which then stays its cover. The second, when a
    // listed spot has another peek spot, is on that one, and the nearest spot sliding there becomes
    // the cover.
    {
        const auto npc_at = [&] { return w.find_object(npc_id)->position(); };
        const auto hidden_at = [&](point p) {
            const auto s = status();
            return s.st == state::hidden && s.cover == p && npc_at() == p;
        };
        const auto exposed_at = [&](point p) {
            const auto s = status();
            return s.st == state::exposed && s.has_peek && s.peek == p && npc_at() == p;
        };
        const auto first = stops[0].pos;
        deadline = _driver->scene_dt + max_dt;
        while (!hidden_at(first))
        {
            fm_assert(_driver->scene_dt < deadline);
            co_yield {};
        }

        const auto v = press(4);
        fm_assert(v.type == base_test::ValueType::u32);
        Array<cover_spot> spots;
        list_cover_spots(*w.find_object<critter>(npc_id), spots);
        fm_assert(v.u32 > 0 && v.u32 < spots.size());
        const auto switched = spots[v.u32];
        fm_assert_equal(switched.pos, status().cover);
        const auto p = find_cover_peek(*w.find_object<critter>(npc_id), switched);
        fm_assert(p);
        // hidden, out and back, as at cover it found itself
        for (const auto exposed : { false, true, false })
        {
            deadline = _driver->scene_dt + max_dt;
            while (!(exposed ? exposed_at(p->pos) : hidden_at(switched.pos)))
            {
                fm_assert(_driver->scene_dt < deadline);
                co_yield {};
            }
        }

        // The box's spots are still those of the search above, since the player hasn't moved.
        fm_assert(listed(overlay, switched.pos));
        struct target { point peek, cover; };
        target targets[2] = { {p->pos, switched.pos}, };
        auto n_targets = 1u;
        Optional<cover_spot> nearest;
        point other_peek;
        for (const auto& x : overlay)
        {
            const auto q = find_cover_peek(*w.find_object<critter>(npc_id), x);
            if (!q || q->pos == p->pos || nearest && q->pos != other_peek)
                continue;
            if (!nearest || point::distance_squared(x.pos, q->pos) < point::distance_squared(nearest->pos, q->pos))
                nearest = x;
            other_peek = q->pos;
        }
        if (nearest)
            targets[n_targets++] = {other_peek, nearest->pos};
        for (auto t = 0u; t < n_targets; t++)
        {
            const auto [peek, cover] = targets[t];
            set_cursor_at(peek);
            co_await click_at_cursor(mouse_button_left);
            {
                const auto s = status();
                fm_assert(s.st == state::to_peek);
                fm_assert(s.cover == cover && s.has_peek && s.peek == peek);
            }
            deadline = _driver->scene_dt + max_dt;
            while (!exposed_at(peek))
            {
                fm_assert(_driver->scene_dt < deadline);
                co_yield {};
            }
            deadline = _driver->scene_dt + max_dt;
            while (!hidden_at(cover))
            {
                fm_assert(_driver->scene_dt < deadline);
                co_yield {};
            }
        }

        set_cursor_at(first);
        co_await click_at_cursor(mouse_button_left);
        fm_assert(status().st == state::to_cover);
        fm_assert_equal(first, status().cover);
        deadline = _driver->scene_dt + max_dt;
        while (!hidden_at(first))
        {
            fm_assert(_driver->scene_dt < deadline);
            co_yield {};
        }
        fm_debug("cover-ai: switched to spot %u of %u, %u peek spots clicked", v.u32, (uint32_t)spots.size(), n_targets);
    }

    // The player moves south while the npc peeks. From there the wall hides the gun as well as the
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

// Walls in six places stand between the npc and the player. The player keeps moving to where it
// sees the npc. After each move, the npc hides at a new spot the player can't see, then peeks from
// a spot from which the player sees its gun, and goes back.
task app::scene_cover_ai_map()
{
    using tests::base_test;
    using tests::Test;
    using state = cover_script_status::state;

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
    uint32_t at = 0;
    const auto teleport_player = [&](uint32_t k) {
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
        const auto s = get_cover_script_status(*C);
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
    for (auto k = 0u; ; k++)
    {
        const auto player = tile_pos(map_player_tiles[at]);
        auto deadline = _driver->scene_dt + max_dt;
        for (;;)
        {
            if (const auto s = status(); s.st == state::hidden && npc_pos() == s.cover && (!k || s.cover != covers.back()))
                break;
            if (_driver->scene_dt >= deadline)
            {
                const auto s = status();
                const auto [x, y] = print(npc_pos());
                const auto [cx, cy] = print(s.cover);
                const auto [tx, ty] = print(player);
                fm_abort("cover-ai-map: move %u, player (%d, %d): the npc doesn't hide at a new spot: "
                         "state %u at (%d, %d), cover (%d, %d), has_cover %d, searches %u",
                         k, tx, ty, (unsigned)s.st, x, y, cx, cy, (int)s.has_cover, s.searches);
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

        uint32_t n_spots, n_peeks = 0;
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
            Array<cover_spot> spots;
            overlay_spots(C, player, spots);
            fm_assert_equal(n_spots, (uint32_t)spots.size());
            fm_assert(listed(spots, cover));
            for (const auto& x : spots)
            {
                fm_assert(!sees(player, x.pos));
                n_peeks += !!find_cover_peek(C, x);
            }
        }

        deadline = _driver->scene_dt + max_dt;
        for (;;)
        {
            if (const auto s = status(); s.st == state::exposed && npc_pos() == s.peek)
                break;
            if (_driver->scene_dt >= deadline)
            {
                const auto [x, y] = print(cover);
                fm_abort("cover-ai-map: move %u: the npc doesn't peek from cover (%d, %d)", k, x, y);
            }
            co_yield {};
        }
        {
            const auto s = status();
            fm_assert_equal(player, s.gun_seen_from);
            fm_assert(sees(player, s.gun));
            const auto [cx, cy] = print(cover);
            const auto [px, py] = print(s.peek);
            const auto [tx, ty] = print(player);
            fm_debug("cover-ai-map: move %u, player (%d, %d): cover (%d, %d), peek (%d, %d), %u spots listed, "
                     "%u with peek spots, whole search %.0f us", k, tx, ty, cx, cy, px, py, n_spots, n_peeks,
                     (double)Time::to_milliseconds(box_time)*1e3);
        }
        if (k == n_moves)
            break;

        deadline = _driver->scene_dt + max_dt;
        while (!(status().st == state::hidden && npc_pos() == cover))
        {
            fm_assert(_driver->scene_dt < deadline);
            co_yield {};
        }
        auto next = (uint32_t)-1;
        for (auto j = 1u; j < array_size(map_player_tiles); j++)
        {
            const auto i = (at + j) % (uint32_t)array_size(map_player_tiles);
            const auto pt = tile_pos(map_player_tiles[i]);
            if (point::distance(pt, cover) >= map_min_player_dist && sees(pt, cover))
            {
                next = i;
                break;
            }
        }
        if (next == (uint32_t)-1)
        {
            const auto [x, y] = print(cover);
            fm_abort("cover-ai-map: move %u: no player tile sees cover (%d, %d)", k, x, y);
        }
        teleport_player(next);
        co_yield {};
    }

    auto distinct = 0u;
    for (auto i = 0u; i < covers.size(); i++)
    {
        auto j = 0u;
        while (j < i && covers[j] != covers[i])
            j++;
        distinct += j == i;
    }
    fm_debug("cover-ai-map: %u moves, %u cover spots, %u ms", n_moves, distinct, (uint32_t)Time::to_milliseconds(_driver->scene_dt));

    tests().switch_to(Test::none);
    do_key(key_mode_none);
    co_yield {};
}

} // namespace floormat

#endif
