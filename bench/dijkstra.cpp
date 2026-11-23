#include "cover-world.hpp"
#include "src/search-result.hpp"
#include "src/search-astar.hpp"
#include "src/grid-pass.hpp"
#include "src/critter.hpp"
#include "src/critter-script-cover-search.hpp"
#include "src/critter-script-cover-spot-tests.hpp"
#include "src/order-list.hpp"
#include "src/scenery-proto.hpp"
#include "src/tile-defs.hpp"
#include "src/tile-image.hpp"
#include "src/update-world.hpp"
#include "src/point.inl"
#include "src/wall-atlas.hpp"
#include "src/world.hpp"
#include "compat/borrowed-ptr.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/function-view.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include <benchmark/benchmark.h>
#include <cr/Optional.h>
#include <mg/Functions.h>

namespace floormat {

namespace {

constexpr inline auto ch = chunk_coords_{0, 0, 0};
constexpr inline auto npc_tile = global_coords{ch, local_coords{2, 8}};
constexpr inline auto body = Vector2ui{24, 24};
// Retreat spots lie within 5 tiles of the npc.
constexpr inline int retreat_spot_dists[] = {3*tile_size_xy, 4*tile_size_xy, 5*tile_size_xy};
// Distances of the cover search's spots. 45 tiles is cover_search::radius, 2 chunk diagonals, in whole tiles.
constexpr inline int cover_spot_dists[] = {chunk_size<int>, 2*chunk_size<int>, 45*tile_size_xy};
// Narrower than the npc's body, so no path leads through the slit.
constexpr inline int slit_width = 8;

tile_ref tile_at(world& w, global_coords g) { return w[g.chunk3()][g.local()]; }

global_coords target_tile(int dist) { return npc_tile + Vector2i{dist/tile_size_xy, 0}; }

// Returns the semi-axes of the ellipse of points whose distances to npc_tile and target_tile(dist) add up to
// path_to's max_dist. The search adds no node whose f is max_dist or more.
Vector2i search_area_semi_axes(int dist)
{
    const auto r = (int)Cover::max_search_path_length(point{npc_tile, {}}, point{target_tile(dist), {}})/2;
    const auto d = dist/2;
    return {r, (int)Math::sqrt((float)(r*r - d*d))};
}

// Calls fun for the cover chunks and for each chunk within a tile of where a path_to from npc_tile to
// target_tile(dist) may add a node.
template<typename F> void for_search_area_chunks(int dist, F&& fun)
{
    const auto a = Vector3i(point{npc_tile, {}}).xy(), b = Vector3i(point{target_tile(dist), {}}).xy();
    const auto mid = (a + b)/2, half_size = search_area_semi_axes(dist) + Vector2i{tile_size_xy};
    const auto chunk_of = [](Vector2i px) {
        return Vector2i{floor_div<chunk_size<int>>(px.x() + half_tile<int>),
                        floor_div<chunk_size<int>>(px.y() + half_tile<int>)};
    };
    const auto lo = Math::min(chunk_of(mid - half_size), Vector2i{-1});
    const auto hi = Math::max(chunk_of(mid + half_size), Vector2i{1});
    for (auto y = lo.y(); y <= hi.y(); y++)
        for (auto x = lo.x(); x <= hi.x(); x++)
            fun(chunk_coords_{(i16)x, (i16)y, 0});
}

// Lays ground over for_search_area_chunks(dist), around make_cover_world()'s chunks when scenery is set.
void make_search_area(world& w, int dist, bool scenery)
{
    if (scenery)
        make_cover_world(w);
    const auto floor = tile_image_proto{loader.ground_atlas("tiles"), 0};
    for_search_area_chunks(dist, [&](chunk_coords_ coord) {
        if (!w.at(coord))
        {
            auto& c = w[coord];
            for (auto j = 0u; j < TILE_MAX_DIM; j++)
                for (auto i = 0u; i < TILE_MAX_DIM; i++)
                    c[{i, j}].ground() = floor;
        }
    });
}

void rebuild_passability(world& w)
{
    for (auto& c : w.chunks())
    {
        c.mark_passability_modified();
        c.ensure_passability();
    }
}

void Dijkstra(benchmark::State& state)
{
    (void)loader.wall_atlas_list();
    auto w = world();
    auto A = astar();

    constexpr auto wcx = 1, wcy = 1, wtx = 8, wty = 8, wox = 0, woy = 0;
    constexpr auto max_dist = (u32)(Vector2i(Math::abs(wcx)+1, Math::abs(wcy)+1)*chunk_size<Vector2i>).length();
    constexpr auto wch = chunk_coords_{wcx, wcy, 0};
    constexpr auto wt = local_coords{wtx, wty};
    constexpr auto wpos = global_coords{wch, wt};
    const auto from = point{{0, 0, 0}, {11, 9}};
    constexpr auto own_size = Vector2ui{16};
    const auto cell = Pass::Params{astar::default_div_size, own_size.x(), pass_mode::pass}.div_size();

    // Same floors as add_floors() in test/dijkstra.cpp.
    const auto floor = tile_image_proto{ loader.ground_atlas("tiles"), 0 };
    const auto reach = Vector2i{(int)(max_dist + cell)};
    const auto lo = (from - reach).chunk(), hi = (from + reach).chunk();
    for (auto j = lo.y; j <= hi.y; j++)
        for (auto i = lo.x; i <= hi.x; i++)
        {
            auto& c = w[{i, j, 0}];
            for (auto y = 0u; y < TILE_MAX_DIM; y++)
                for (auto x = 0u; x < TILE_MAX_DIM; x++)
                    c[{x, y}].ground() = floor;
            c.mark_passability_modified();
            c.ensure_passability();
        }

    auto& ch = w[wch];
    auto metal2 = wall_image_proto{loader.wall_atlas("empty", loader_policy::warn), 0};

    for (i16 j = wcy - 1; j <= wcy + 1; j++)
        for (i16 i = wcx - 1; i <= wcx + 1; i++)
        {
            auto &c = w[{i, j, 0}];
            for (int k : {  3, 4, 5, 6, 11, 12, 13, 14, 15, })
            {
                c[{ k, k }].wall_north() = metal2;
                c[{ k, k }].wall_west() = metal2;
            }
        }

    ch[{ wtx,   wty   }].wall_west()  = metal2;
    ch[{ wtx,   wty   }].wall_north() = metal2;
    ch[{ wtx+1, wty   }].wall_west()  = metal2;
    ch[{ wtx,   wty +1}].wall_north() = metal2;

    for (i16 j = wcy - 1; j <= wcy + 1; j++)
        for (i16 i = wcx - 1; i <= wcx + 1; i++)
        {
            auto& c = w[{i, j, 0}];
            c.mark_passability_modified();
            c.ensure_passability();
        }

    auto run = [&]<int Debug> {
        return A.Dijkstra<Debug>(w, from,
                                 {wpos, {wox, woy}},   // to
                                 max_dist, own_size,
                                 Search::without_critters);
    };

    {
        auto res = run.operator()<0>();
        fm_assert(!res.is_found());
        fm_assert(res.distance() < 128);
        fm_assert(res.distance() > 8);
        fm_assert(res.cost() > 1800);
        fm_assert(res.cost() < 3000);
    }

    for (int i = 0; i < 3; i++)
        run.operator()<0>();
    for (auto _ : state)
        run.operator()<0>();
}

template<typename F>
void time_spot_tests_call(benchmark::State& st, F&& find_path)
{
    const auto dist = (int)st.range(0);
    const bool walled_in = st.range(1), scenery = st.range(2);
    const auto to_tile = target_tile(dist);

    auto w = world();
    make_search_area(w, dist, scenery);
    if (walled_in)
    {
        const auto wall = wall_image_proto{loader.wall_atlas("empty"), 0};
        tile_at(w, to_tile).wall_north() = wall;
        tile_at(w, to_tile).wall_west() = wall;
        tile_at(w, to_tile + Vector2i{1, 0}).wall_west() = wall;
        tile_at(w, to_tile + Vector2i{0, 1}).wall_north() = wall;
    }
    rebuild_passability(w);

    critter_proto p;
    p.bbox_size = Vector2ub(body);
    const auto npc = w.make_object<critter>(w.make_id(), npc_tile, move(p));
    auto A = astar{};
    w.set_grid_registry(&A.pool_registry());
    auto t = Cover::spot_tests{A, 0, NullOpt};
    const auto to = point{to_tile, {}};
    // The read-only calls need the object pool, which this call creates.
    (void)t.path_to(*npc, to);

    fm_assert(find_path(t, *npc, to) == !walled_in);
    const auto added_node_count = t.last_dijkstra_node_count();
    fm_assert(added_node_count > 0);
    // Builds the grids that a read-only call ordered, so the timed calls read only built grids.
    prepare_world_for_thinking(w);
    for (int i = 0; i < 2; i++)
        (void)find_path(t, *npc, to);
    fm_assert(w.grid_orders().isEmpty());
    for (auto _ : st)
        benchmark::DoNotOptimize(find_path(t, *npc, to));
    st.counters["added_node_count"] = added_node_count;
    w.set_grid_registry(nullptr);
}

void Dijkstra_Path_To(benchmark::State& st)
{
    time_spot_tests_call(st, [](Cover::spot_tests& t, critter& c, point to) {
        return t.path_to(c, to).is_found();
    });
}

void Dijkstra_Reachable_Without_Critters(benchmark::State& st)
{
    time_spot_tests_call(st, [](Cover::spot_tests& t, critter& c, point to) {
        auto& w = c.world();
        auto ctx = think_context{w.frame_order_buffer(), w.grid_orders()};
        return t.is_reachable_without_critters(c, to, ctx);
    });
}

struct wall_scene
{
    bptr<critter> npc, threat;
    point to;
};

// Lays walls on the west edges of a column of tiles when is_west, else on the north edges of a row. They run from
// lo to hi px along the line from the center of tile `at`. A tile the line covers in part gets a cut collider.
void add_wall_line(world& w, global_coords at, bool is_west, int lo, int hi)
{
    const auto wall = wall_image_proto{loader.wall_atlas("empty"), 0};
    const auto depth = (int)wall.atlas->info().depth;
    const auto along = is_west ? Vector2i{0, 1} : Vector2i{1, 0};
    const auto k0 = floor_div<tile_size_xy>(lo + half_tile<int>);
    const auto k1 = floor_div<tile_size_xy>(hi + half_tile<int> - 1);
    for (auto k = k0; k <= k1; k++)
    {
        const auto tile = at + along*k;
        const auto a = Math::max(lo, k*tile_size_xy - half_tile<int>);
        const auto b = Math::min(hi, k*tile_size_xy + half_tile<int>);
        if (b - a == tile_size_xy)
        {
            if (is_west)
                tile_at(w, tile).wall_west() = wall;
            else
                tile_at(w, tile).wall_north() = wall;
        }
        else if (b > a)
        {
            scenery_proto p;
            p.atlas = loader.invalid_scenery_atlas().proto->atlas;
            p.subtype = generic_scenery_proto{};
            const auto across = -half_tile<int> - depth/2, middle = a + (b - a)/2 - k*tile_size_xy;
            p.bbox_size = is_west ? Vector2ub{(u8)depth, (u8)(b - a)} : Vector2ub{(u8)(b - a), (u8)depth};
            p.bbox_offset = is_west ? Vector2b{(i8)across, (i8)middle} : Vector2b{(i8)middle, (i8)across};
            p.pass = pass_mode::blocked;
            w.make_scenery(w.make_id(), tile, move(p));
        }
    }
}

// Replaces the west or north wall of `tile` with colliders that leave a gap width px wide in its middle.
void add_slit(world& w, global_coords tile, bool is_west, int width)
{
    if (is_west)
        tile_at(w, tile).wall_west() = wall_image_proto{};
    else
        tile_at(w, tile).wall_north() = wall_image_proto{};
    add_wall_line(w, tile, is_west, -half_tile<int>, -width/2);
    add_wall_line(w, tile, is_west, width - width/2, half_tile<int>);
}

// Lays a U-shaped wall, open to the east, around num_rows_inside rows of tiles centered on tile `at`. Its west
// side lies on the west edge of at's column, and the other two run side_length px east from there.
void add_u_shaped_wall(world& w, global_coords at, int num_rows_inside, int side_length)
{
    const auto half = num_rows_inside/2;
    const auto lo = -half_tile<int>, hi = side_length - half_tile<int>;
    add_wall_line(w, at, true, lo - half*tile_size_xy, half*tile_size_xy - lo);
    add_wall_line(w, at - Vector2i{0, half}, false, lo, hi);
    add_wall_line(w, at + Vector2i{0, half + 1}, false, lo, hi);
}

// Returns the npc on npc_tile, the target dist px east of it and the threat at `threat`. It rebuilds passability,
// so every wall goes in before the call.
wall_scene add_critters(world& w, int dist, point threat)
{
    rebuild_passability(w);
    critter_proto p;
    p.bbox_size = Vector2ub(body);
    auto npc = w.make_object<critter>(w.make_id(), npc_tile, critter_proto{p});
    p.offset = threat.offset();
    auto threat_critter = w.make_object<critter>(w.make_id(), threat.coord(), move(p));
    return {move(npc), move(threat_critter), point{target_tile(dist), {}}};
}

// Returns add_critters() with the threat walled into a room of 2x2 tiles south of the npc and the target, on which
// make_cover_world() puts no scenery or critter.
wall_scene add_critters_with_threat_walled_in(world& w, int dist)
{
    const auto wall = wall_image_proto{loader.wall_atlas("empty"), 0};
    const auto room = npc_tile + Vector2i{2, 9};
    for (auto i = 0; i < 2; i++)
    {
        tile_at(w, room + Vector2i{i, 0}).wall_north() = wall;
        tile_at(w, room + Vector2i{i, 2}).wall_north() = wall;
        tile_at(w, room + Vector2i{0, i}).wall_west() = wall;
        tile_at(w, room + Vector2i{2, i}).wall_west() = wall;
    }
    return add_critters(w, dist, point{room + Vector2i{1, 1}, Vector2b{(i8)-half_tile<int>}});
}

// Returns add_critters() with the threat in a room of 3x3 tiles around `center`, from which it sees out only
// through a slit in the west or north wall of slit_tile.
wall_scene add_critters_with_threat_behind_slit(world& w, int dist, global_coords center, global_coords slit_tile,
                                                bool is_west)
{
    constexpr auto r = 3*half_tile<int>;
    add_wall_line(w, center - Vector2i{0, 1}, false, -r, r);
    add_wall_line(w, center + Vector2i{0, 2}, false, -r, r);
    add_wall_line(w, center - Vector2i{1, 0}, true, -r, r);
    add_wall_line(w, center + Vector2i{2, 0}, true, -r, r);
    add_slit(w, slit_tile, is_west, slit_width);
    return add_critters(w, dist, point{center, {}});
}

// Returns add_critters() with a line of west walls between the npc and the target that reaches wall_half_length px
// north and south of their row. With is_target_exposed, the threat sees the target through a slit east of it.
wall_scene make_wall_scene(world& w, int dist, bool scenery, int wall_half_length, bool is_target_exposed)
{
    make_search_area(w, dist, scenery);
    // The wall lies halfway between npc and target, or 32 px nearer the npc for an even number of tiles.
    // Among scenery it lies 32 px nearer the target instead, away from the box at tile (3, 13).
    const auto column = npc_tile + Vector2i{(dist/tile_size_xy + 1 + scenery)/2, 0};
    add_wall_line(w, column, true, -wall_half_length, wall_half_length);
    if (is_target_exposed)
    {
        const auto room = target_tile(dist) + Vector2i{2, 0};
        return add_critters_with_threat_behind_slit(w, dist, room, room - Vector2i{1, 0}, true);
    }
    else
        return add_critters_with_threat_walled_in(w, dist);
}

// Returns add_critters() with the target inside a U-shaped wall that opens away from the npc, whose sides are
// side_length px long. With is_target_exposed, the threat sees the target through a slit north of it.
wall_scene make_u_shaped_wall_scene(world& w, int dist, bool scenery, int side_length, bool is_target_exposed)
{
    make_search_area(w, dist, scenery);
    // Among scenery, the box at tile (8, 8) blocks a U with one row of tiles inside.
    const auto num_rows_inside = scenery ? 3 : 1;
    const auto to_tile = target_tile(dist);
    add_u_shaped_wall(w, to_tile, num_rows_inside, side_length);
    if (is_target_exposed)
    {
        const auto north_side = to_tile - Vector2i{0, num_rows_inside/2};
        return add_critters_with_threat_behind_slit(w, dist, north_side - Vector2i{0, 2}, north_side, false);
    }
    else
        return add_critters_with_threat_walled_in(w, dist);
}

// Returns the largest length below hi with which path_to finds a path in the scene make_scene(w, length) returns.
template<typename F> int max_length_with_path(int hi, F&& make_scene)
{
    const auto is_found = [&](int length) {
        auto w = world();
        auto A = astar{};
        const auto s = make_scene(w, length);
        auto t = Cover::spot_tests{A, s.threat->id, NullOpt};
        return t.path_to(*s.npc, s.to).is_found();
    };
    auto lo = 0;
    fm_assert(is_found(lo));
    fm_assert(!is_found(hi));
    while (hi - lo > 1)
    {
        const auto mid = (lo + hi)/2;
        if (is_found(mid))
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

// Returns the largest wall_half_length with which path_to finds a path in make_wall_scene().
int max_wall_half_length(int dist, bool scenery, bool is_target_exposed)
{
    // A wall whose half-length is the semi-minor axis leaves no way around it within max_dist.
    return max_length_with_path(search_area_semi_axes(dist).y(), [&](world& w, int length) {
        return make_wall_scene(w, dist, scenery, length, is_target_exposed);
    });
}

// Returns the largest side_length with which path_to finds a path in make_u_shaped_wall_scene().
int max_u_shaped_wall_side_length(int dist, bool scenery, bool is_target_exposed)
{
    // The way around sides max_dist px long is longer than max_dist.
    const auto max_dist = (int)Cover::max_search_path_length(point{npc_tile, {}}, point{target_tile(dist), {}});
    return max_length_with_path(max_dist, [&](world& w, int length) {
        return make_u_shaped_wall_scene(w, dist, scenery, length, is_target_exposed);
    });
}

void time_path_to(benchmark::State& st, astar& A, const wall_scene& s, bool found)
{
    auto t = Cover::spot_tests{A, s.threat->id, NullOpt};
    const auto path = t.path_to(*s.npc, s.to);
    fm_assert(path.is_found() == found);
    const auto added_node_count = t.last_dijkstra_node_count();
    for (int i = 0; i < 2; i++)
        (void)t.path_to(*s.npc, s.to);
    for (auto _ : st)
        benchmark::DoNotOptimize(t.path_to(*s.npc, s.to).is_found());
    st.counters["added_node_count"] = added_node_count;
    st.counters["cost"] = path.cost();
    st.counters["max_dist"] = Cover::max_search_path_length(s.npc->position(), s.to);
}

void time_is_seen_on_path(benchmark::State& st, astar& A, const wall_scene& s, bool is_seen)
{
    auto& w = s.npc->world();
    w.set_grid_registry(&A.pool_registry());
    auto t = Cover::spot_tests{A, s.threat->id, NullOpt};
    const auto path = t.path_to(*s.npc, s.to);
    fm_assert(path.is_found());

    const Cover::Script::options o{};
    const auto threat = s.threat->position();
    auto ctx = think_context{w.frame_order_buffer(), w.grid_orders()};
    // The timed calls reuse the threat lattice that this one computes.
    fm_assert(t.is_seen_on_path(*s.npc, o, threat, path, ctx) == is_seen);
    // Builds the grids that call ordered, so the timed calls read only built grids.
    prepare_world_for_thinking(w);
    const auto body_checks_before = t.counters.num_path_body_checks;
    fm_assert(t.is_seen_on_path(*s.npc, o, threat, path, ctx) == is_seen);
    const auto body_checks_per_call = t.counters.num_path_body_checks - body_checks_before;
    fm_assert(w.grid_orders().isEmpty());
    for (auto _ : st)
        benchmark::DoNotOptimize(t.is_seen_on_path(*s.npc, o, threat, path, ctx));
    st.counters["body_checks_per_call"] = body_checks_per_call;
    w.set_grid_registry(nullptr);
}

void Dijkstra_Path_To_Around_Wall(benchmark::State& st)
{
    const auto dist = (int)st.range(0);
    const bool scenery = st.range(1), found = st.range(2);
    // Without found, the wall is 1 px longer than the longest that leaves a path within max_dist.
    const auto wall_half_length = max_wall_half_length(dist, scenery, false) + !found;
    auto w = world();
    auto A = astar{};
    time_path_to(st, A, make_wall_scene(w, dist, scenery, wall_half_length, false), found);
    st.counters["wall_half_length"] = wall_half_length;
}

void Dijkstra_Path_To_Inside_U_Shaped_Wall(benchmark::State& st)
{
    const auto dist = (int)st.range(0);
    const bool scenery = st.range(1);
    const auto side_length = max_u_shaped_wall_side_length(dist, scenery, false);
    auto w = world();
    auto A = astar{};
    time_path_to(st, A, make_u_shaped_wall_scene(w, dist, scenery, side_length, false), true);
    st.counters["side_length"] = side_length;
}

void Dijkstra_Is_Seen_On_Path_Around_Wall(benchmark::State& st)
{
    const auto dist = (int)st.range(0);
    const bool scenery = st.range(1);
    auto w = world();
    auto A = astar{};
    const auto s = make_wall_scene(w, dist, scenery, max_wall_half_length(dist, scenery, false), false);
    time_is_seen_on_path(st, A, s, false);
}

void Dijkstra_Is_Seen_Near_Target_Around_Wall(benchmark::State& st)
{
    const auto dist = (int)st.range(0);
    auto w = world();
    auto A = astar{};
    const auto s = make_wall_scene(w, dist, false, max_wall_half_length(dist, false, true), true);
    time_is_seen_on_path(st, A, s, true);
}

void Dijkstra_Is_Seen_Near_Target_Inside_U_Shaped_Wall(benchmark::State& st)
{
    const auto dist = (int)st.range(0);
    auto w = world();
    auto A = astar{};
    const auto side_length = max_u_shaped_wall_side_length(dist, false, true);
    const auto s = make_u_shaped_wall_scene(w, dist, false, side_length, true);
    time_is_seen_on_path(st, A, s, true);
}

void spot_tests_args(benchmark::Benchmark* b)
{
    b->ArgNames({"dist", "walled_in", "scenery"})
        ->ArgsProduct({{3*tile_size_xy, 4*tile_size_xy, 5*tile_size_xy}, {0, 1}, {0, 1}})
        ->Unit(benchmark::kMicrosecond);
}

void cover_search_args(benchmark::Benchmark* b)
{
    b->ArgNames({"dist", "walled_in", "scenery"});
    for (const auto dist : cover_spot_dists)
        for (const auto walled_in : {0, 1})
            b->Args({dist, walled_in, 0});
    b->Unit(benchmark::kMicrosecond);
}

void path_to_around_wall_args(benchmark::Benchmark* b)
{
    b->ArgNames({"dist", "scenery", "found"});
    for (const auto dist : retreat_spot_dists)
        for (const auto scenery : {0, 1})
            for (const auto found : {1, 0})
                b->Args({dist, scenery, found});
    for (const auto dist : cover_spot_dists)
        for (const auto found : {1, 0})
            b->Args({dist, 0, found});
    b->Unit(benchmark::kMicrosecond);
}

void u_shaped_wall_args(benchmark::Benchmark* b)
{
    b->ArgNames({"dist", "scenery"});
    for (const auto dist : retreat_spot_dists)
        b->Args({dist, 0});
    // At 192 and 256 px, make_cover_world()'s scenery closes the U before its path nears max_dist.
    b->Args({retreat_spot_dists[2], 1});
    b->Unit(benchmark::kMicrosecond);
}

void wall_scene_args(benchmark::Benchmark* b)
{
    b->ArgNames({"dist", "scenery"});
    for (const auto dist : retreat_spot_dists)
        for (const auto scenery : {0, 1})
            b->Args({dist, scenery});
    for (const auto dist : cover_spot_dists)
        b->Args({dist, 0});
    b->Unit(benchmark::kMicrosecond);
}

void target_exposed_args(benchmark::Benchmark* b)
{
    // Among make_cover_world()'s scenery, the threat's room would hold boxes.
    b->ArgNames({"dist"});
    for (const auto dist : retreat_spot_dists)
        b->Args({dist});
    b->Unit(benchmark::kMicrosecond);
}

} // namespace

BENCHMARK(Dijkstra)->Unit(benchmark::kMillisecond);
BENCHMARK(Dijkstra_Path_To)->Apply(spot_tests_args);
BENCHMARK(Dijkstra_Reachable_Without_Critters)->Apply(spot_tests_args);
BENCHMARK(Dijkstra_Path_To)->Apply(cover_search_args);
BENCHMARK(Dijkstra_Reachable_Without_Critters)->Apply(cover_search_args);
BENCHMARK(Dijkstra_Path_To_Around_Wall)->Apply(path_to_around_wall_args);
BENCHMARK(Dijkstra_Path_To_Inside_U_Shaped_Wall)->Apply(u_shaped_wall_args);
BENCHMARK(Dijkstra_Is_Seen_On_Path_Around_Wall)->Apply(wall_scene_args);
BENCHMARK(Dijkstra_Is_Seen_Near_Target_Around_Wall)->Apply(target_exposed_args);
BENCHMARK(Dijkstra_Is_Seen_Near_Target_Inside_U_Shaped_Wall)->Apply(target_exposed_args);

} // namespace floormat
