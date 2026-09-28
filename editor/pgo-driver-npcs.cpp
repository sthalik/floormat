#include "pgo-driver.hpp"
#include "app.hpp"
#include "loader/loader.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile.hpp"
#include "src/wall-atlas.hpp"
#include "src/critter.hpp"
#include "src/critter-script.hpp"
#include "src/scenery.hpp"
#include "src/scenery-proto.hpp"
#include "src/search-astar.hpp"
#include "src/search-constants.hpp"
#include "src/search-pred.hpp"
#include "src/search-result.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-pass-pool.hpp"
#include "src/point.inl"
#include "src/nanosecond.inl"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/function2.hpp"
#include "floormat/main.hpp"
#include <mg/Functions.h>

#ifndef FLOORMAT_NO_PGO_DRIVER

namespace floormat {

using pgo::task;

namespace {

constexpr int16_t room_chunks = fm_ASAN ? 2 : 8;
constexpr uint32_t num_npcs = fm_ASAN ? 64 : 1024;
constexpr uint32_t num_walkers = num_npcs/8, num_critters = num_npcs + num_walkers;
constexpr uint32_t lattice_side = [] {
    uint32_t n = 1;
    while (n*n < num_critters)
        n++;
    return n;
}();
constexpr uint32_t num_blockers = num_npcs*3/16;
constexpr uint8_t critter_size = 32, blocker_size = 32;
constexpr float speed = 4;
constexpr auto hold_dt = Second*20, blocker_lifetime = Second*5;

struct actor
{
    object_id id;
    point dest, last;
    bool walker, vertical, forward;
};

struct blocker
{
    object_id id = 0;
    Ns placed_at{};
};

} // namespace

task app::scene_npcs()
{
    constexpr int16_t r0 = 0, r1 = room_chunks;
    constexpr int margin = Search::div_size.x();

    auto& w = M->world();
    auto wall = loader.wall_atlas("test1", loader_policy::warn);
    const auto depth = (int)wall->info().depth;

    chunk* room[room_chunks*room_chunks];
    for (int16_t cy = r0; cy < r1; cy++)
        for (int16_t cx = r0; cx < r1; cx++)
        {
            auto& c = w[chunk_coords_{cx, cy, 0}];
            maybe_initialize_chunk_(c.coord(), c);
            room[(cy - r0)*room_chunks + (cx - r0)] = &c;
        }
    for (int16_t k = r0; k < r1; k++)
    {
        auto& north = w[chunk_coords_{k, r0, 0}];
        auto& south = w[chunk_coords_{k, r1, 0}];
        auto& west = w[chunk_coords_{r0, k, 0}];
        auto& east = w[chunk_coords_{r1, k, 0}];
        for (uint8_t i = 0; i < TILE_MAX_DIM; i++)
        {
            north[local_coords{i, 0}].wall_north() = { wall, (variant_t)-1 };
            south[local_coords{i, 0}].wall_north() = { wall, (variant_t)-1 };
            west[local_coords{0, i}].wall_west() = { wall, (variant_t)-1 };
            east[local_coords{0, i}].wall_west() = { wall, (variant_t)-1 };
        }
        north.mark_modified();
        south.mark_modified();
        west.mark_modified();
        east.mark_modified();
    }

    // A wall's box lies outside its tile, so the interior is [-half_tile, room - half_tile - depth).
    const int origin = r0*chunk_size<int>;
    const int lo = origin + margin, hi = origin + room_chunks*chunk_size<int> - tile_size_xy - depth - margin;
    const int step = (hi - lo)/(int)(lattice_side - 1);
    fm_assert(step > critter_size);
    const auto at = [](Vector2i px) { return point{Vector3i{px.x(), px.y(), 0}}; };

    // reset_world_post() spawned the player at (0,0), inside the room.
    auto player = ensure_player_character(w);
    auto player_index = player->index();
    player->teleport_to(player_index, point{chunk_coords_{-1, -1, 0}, local_coords{}, {}}, rotation_COUNT);

    Array<actor> actors{ValueInit, num_critters};
    for (auto i = 0u; i < num_critters; i++)
    {
        const bool walker = i % 9 == 0;
        const auto pt = at({lo + (int)(i % lattice_side)*step, lo + (int)(i / lattice_side)*step});
        critter_proto p;
        p.name = walker ? "walker"_s : "npc"_s;
        p.speed = speed;
        p.bbox_size = Vector2ub{critter_size};
        p.offset = pt.offset();
        fm_assert(w[pt.chunk3()].can_place_object(p, pt.local()));
        const auto id = w.make_object<critter>(w.make_id(), pt.coord(), move(p))->id;
        actors[i] = actor{ .id = id, .dest = pt, .last = pt, .walker = walker,
                           .vertical = walker && i/9 % 2 != 0, .forward = false, };
    }

    // update_world() otherwise only updates the chunks around the screen.
    _update_all_chunks = true;
    center_camera_on(at(Vector2i{(lo + hi)/2}));
    M->reset_fps();

    auto blocker_proto = loader.scenery("stool1");
    blocker_proto.bbox_offset = {};
    blocker_proto.bbox_size = Vector2ub{blocker_size};
    blocker_proto.pass = pass_mode::blocked;
    blocker blockers[num_blockers]{};

    // Its own pool: grids built through this pred would put critters into every later search of
    // the same size.
    Pass::Pool crit_pool{w.pass_pool_registry().pool_for(critter_size).params()};
    object_id searcher = 0;
    const auto except_searcher = [&searcher](chunk&, collision_data data, Range2D) {
        return data.type == (uint64_t)collision_type::scenery && data.id == searcher
               ? path_search_continue::pass : path_search_continue::blocked;
    };
    const Search::pred except_searcher_pred{except_searcher};

    uint64_t rng = 0x9e3779b97f4a7c15u;
    const auto next_angle = [&rng] {
        rng = rng*6364136223846793005u + 1442695040888963407u;
        return Rad{(float)(rng >> 40) * (2*Math::Constants<float>::pi() / (float)(1u << 24))};
    };
    const auto max_dist = [](point a, point b) { return point::distance(a, b)*2 + chunk_size_xy; };

    uint32_t frames = 0, moving = 0, walks = 0, collisions = 0, searches = 0, critter_searches = 0;
    uint32_t not_found = 0, blockers_placed = 0;
    bool placed = false;

    const auto place_blocker = [&](const path_search_result& res) {
        auto& slot = blockers[blockers_placed % num_blockers];
        if (placed || res.size() < 3 || (slot.id && _driver->scene_dt - slot.placed_at < blocker_lifetime))
            return;
        const auto mid = res.path()[res.size()/2];
        const auto v = mid - point{};
        if ((v < Vector2i{lo + tile_size_xy}).any() || (v > Vector2i{hi - tile_size_xy}).any())
            return;
        auto p = blocker_proto;
        p.offset = mid.offset();
        if (!w[mid.chunk3()].can_place_object(p, mid.local()))
            return;
        if (slot.id)
        {
            auto o = w.find_object(slot.id);
            fm_assert(o);
            o->chunk().kill_object(*o, o->index());
        }
        slot = { w.make_scenery(w.make_id(), mid.coord(), move(p))->id, _driver->scene_dt };
        blockers_placed++;
        placed = true;
    };
    const auto start_walk = [&](const bptr<critter>& C, path_search_result&& res) {
        walks++;
        place_blocker(res);
        C->script.do_reassign(critter_script::make_walk_script(move(res)), C);
    };

    do
    {
        placed = false;
        for (auto& a : actors)
        {
            auto C = w.find_object<critter>(a.id);
            fm_assert(C);
            const auto pos = C->position();
            if (pos != a.last)
            {
                moving++;
                a.last = pos;
            }
            if (C->moves.AUTO)
                continue;
            // The walk script ends on arrival or on its first blocked step, and only arrival
            // leaves the critter on dest.
            const bool collided = pos != a.dest;
            collisions += collided;
            const auto p = pos - point{};

            if (a.walker)
            {
                a.forward = !a.forward;
                const int end = a.forward ? hi : lo;
                a.dest = at(a.vertical ? Vector2i{p.x(), end} : Vector2i{end, p.y()});
                walks++;
                C->script.do_reassign(critter_script::make_walk_script(a.dest), C);
                continue;
            }

            if (collided)
            {
                searcher = a.id;
                // Critters move without touching pass_gen, so nothing else marks these stale.
                crit_pool.maybe_mark_stale_all(w.frame_no());
                for (auto* c : room)
                    crit_pool[*c].mark_stale();
                auto res = M->astar().Dijkstra(w, crit_pool, pos, a.dest, max_dist(pos, a.dest),
                                               Vector2ui{C->bbox_size}, except_searcher_pred);
                searches++;
                critter_searches++;
                if (res.is_found())
                {
                    start_walk(C, move(res));
                    continue;
                }
                not_found++;
            }

            const auto theta = next_angle();
            const Vector2 d{Math::cos(theta), Math::sin(theta)};
            const auto goal = at(Math::clamp(Vector2i(Math::round(Vector2(p) + d*(float)(hi - lo))),
                                             Vector2i{lo}, Vector2i{hi}));
            a.dest = pos;
            if (goal == pos)
                continue;
            auto res = M->astar().Dijkstra(w, pos, goal, max_dist(pos, goal),
                                           Vector2ui{C->bbox_size}, Search::without_critters());
            searches++;
            if (!res.is_found())
            {
                not_found++;
                continue;
            }
            a.dest = goal;
            start_walk(C, move(res));
        }
        co_yield {};
        frames++;
    }
    while (_driver->scene_dt < hold_dt);

    fm_debug("npcs: %u critters, %u frames, %.0f%% of critter-frames moving, %u walks, "
             "%u collisions, %u searches (%u with critters), %u not found, %u blockers placed",
             num_critters, frames, (double)moving*100/((double)frames*num_critters), walks,
             collisions, searches, critter_searches, not_found, blockers_placed);
}

} // namespace floormat

#endif
