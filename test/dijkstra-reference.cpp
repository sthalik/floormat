#include "app.hpp"
#include "src/search-astar.hpp"
#include "src/search-result.hpp"
#include "src/search-cache.hpp"
#include "src/search-pred.hpp"
#include "src/search-octile.hpp"
#include "src/grid-pass.hpp"
#include "src/point.inl"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/scenery-proto.hpp"
#include "src/wall-atlas.hpp"
#include "loader/loader.hpp"
#include "loader/scenery-cell.hpp"
#include "loader/wall-cell.hpp"
#include "random/random.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/fill.hpp"
#include "compat/function2.hpp"
#include <cr/GrowableArray.h>
#include <algorithm>

namespace floormat {

namespace {

constexpr int dim = (int)TILE_MAX_DIM, last = 2*dim - 1;

constexpr uint32_t unreached = (uint32_t)-1, max_dist = 6*chunk_size_xy;

// Every cell within this many px of a zone centre is free: the seeds and every node within the
// search's goal_radius(), so the R-tree checks of both pass.
constexpr int clear_px = 2*tile_size_xy;

// off the lattice, where an edge to the goal costing anything but octile changes the cost
struct zone { Vector2i tile; Vector2b offset; };
constexpr zone zones[] = {
    { {4, 4}, {13, 7} }, { {last - 5, 4}, {-11, 5} },
    { {4, last - 5}, {3, -14} }, { {last - 5, last - 5}, {-9, -6} },
};

struct pool_size { uint32_t cell, bbox; };
constexpr pool_size small_pools[] = { {4, 32}, {8, 32}, {16, 32}, {32, 32}, };
// A block closes cells up to ~95 px away at 64-px cells and bbox, so this map has no random blocks.
constexpr pool_size large_pools[] = { {16, 64}, {32, 64}, {64, 64}, };

// Lattice point v is the pixel v*step, a step being one cell. Every node of the search is one:
// seeds sit at from rounded down to the lattice, plus 0 or a step. The zones are clear, so a
// seed always takes, a node is within goal_radius() of the goal, and neither seed search runs.
struct lattice
{
    int step, tile, lo, hi, n, clear_steps;
    Array<uint8_t> free;

    explicit lattice(int step) :
        step{step}, tile{tile_size_xy/step}, lo{-tile}, hi{(last + 1)*tile}, n{hi - lo + 1},
        clear_steps{clear_px/step}, free{ValueInit, (size_t)(n*n)}
    {}

    uint32_t index(Vector2i v) const { return (uint32_t)((v.y() - lo)*n + v.x() - lo); }
    bool inside(Vector2i v) const { return v.x() >= lo && v.x() <= hi && v.y() >= lo && v.y() <= hi; }
    bool at(Vector2i v) const { return inside(v) && free[index(v)]; }
    point pt(Vector2i v) const { return point{Vector3i{v*step, 0}}; }
};

point zone_point(zone z) { return point{Vector3i{z.tile*tile_size_xy + Vector2i(z.offset), 0}}; }

tile_ref tile_at(world& w, int x, int y)
{
    auto& c = w[chunk_coords_{(int16_t)(x / dim), (int16_t)(y / dim), 0}];
    return c[local_coords{(uint8_t)(x % dim), (uint8_t)(y % dim)}];
}

void add_block(world& w, int x, int y, Vector2ub size, Vector2b offset = {})
{
    scenery_proto p;
    p.atlas       = loader.invalid_scenery_atlas().proto->atlas;
    p.subtype     = generic_scenery_proto{};
    p.bbox_size   = size;
    p.bbox_offset = offset;
    p.pass        = pass_mode::blocked;
    w.make_scenery(w.make_id(), {chunk_coords_{(int16_t)(x / dim), (int16_t)(y / dim), 0},
                                 local_coords{(uint8_t)(x % dim), (uint8_t)(y % dim)}}, move(p));
}

bool near_zone(int x, int y)
{
    for (auto z : zones)
        if (Math::max(Math::abs(x - z.tile.x()), Math::abs(y - z.tile.y())) <= 3)
            return true;
    return false;
}

// Routes around a convex corner step diagonally past it, and there only the step's side cells
// are blocked. Straight walls decide the straight steps' target cells.
void make_map(world& w, bool for_large_pools)
{
    const auto wall = wall_image_proto{loader.invalid_wall_atlas().atlas, 0};

    for (int i = 0; i < last; i++)
    {
        tile_at(w, i, 0).wall_north() = wall;
        tile_at(w, i, last).wall_north() = wall;
        tile_at(w, 0, i).wall_west() = wall;
        tile_at(w, last, i).wall_west() = wall;
    }

    // At 64-px cells and bbox, a wall closes the cells on both sides of it, so a gap of k tiles
    // lets k - 2 through. The large pools' map widens every gap by a tile at each end.
    const int widen = for_large_pools ? 1 : 0;
    const auto gap = [=](int i, int from, int to) { return i >= from - widen && i <= to + widen; };
    for (int y = 0; y < last; y++)
        if (!gap(y, 15, 15) && !gap(y, 24, 25) && !gap(y, 4, 6))
            tile_at(w, 10, y).wall_west() = wall;
    for (int x = 10; x < last; x++)
        if (!gap(x, 14, 14) && !gap(x, 19, 20) && !gap(x, 24, 26))
            tile_at(w, x, 12).wall_north() = wall;

    for (int k = 0; k < 6; k++)
    {
        tile_at(w, 14 + k, 9 - k).wall_north() = wall;
        tile_at(w, 14 + k, 9 - k).wall_west() = wall;
    }
    for (int k = 0; k < 7; k++)
    {
        tile_at(w, 13 + k, 16 + k).wall_north() = wall;
        tile_at(w, 14 + k, 16 + k).wall_west() = wall;
    }

    constexpr auto full = Vector2ub{tile_size_xy, tile_size_xy};
    add_block(w, 22, 17, full);
    add_block(w, 23, 18, full);
    add_block(w, 6, 14, full);
    add_block(w, 5, 15, full);
    add_block(w, 13, 4, full);
    add_block(w, 12, 5, full);

    Random::xoshiro256starstar rng;
    Random::seed(rng, 23);
    const auto rnd = [&](uint32_t k) { return (uint32_t)(Random::next(rng) % k); };
    for (int y = 0; y < last; y++)
        for (int x = 0; x < last; x++)
            if (!for_large_pools && !near_zone(x, y) && rnd(6) == 0)
            {
                const auto size = Vector2ub{(uint8_t)(4 + 2*rnd(19)), (uint8_t)(4 + 2*rnd(19))};
                const auto offset = Vector2b{(int8_t)((int)rnd(33) - 16), (int8_t)((int)rnd(33) - 16)};
                add_block(w, x, y, size, offset);
            }

    for (int16_t j = 0; j < 2; j++)
        for (int16_t i = 0; i < 2; i++)
        {
            auto& c = w[chunk_coords_{i, j, 0}];
            c.mark_passability_modified();
            c.ensure_passability();
        }
}

// A straight step needs its target's cell free, a diagonal one also the cells at cur + (dx, 0)
// and cur + (0, dy), as in the search's per-cell tests.
void reference_costs(const lattice& L, zone from, Array<uint32_t>& dist)
{
    const auto oct = Search::octile{(uint32_t)L.step};
    const auto step = L.step, tile = L.tile, lo = L.lo, hi = L.hi, n = L.n;
    struct entry { uint32_t d, i; };
    constexpr auto later = [](entry a, entry b) { return a.d > b.d; };

    fill(arrayView(dist), unreached);
    Array<entry> heap;
    arrayReserve(heap, 1024);

    const auto off = Vector2i(from.offset);
    const auto from_lo = from.tile*tile + Vector2i{floor_div(off.x(), step), floor_div(off.y(), step)};
    for (int j = -1; j <= 1; j++)
        for (int i = -1; i <= 1; i++)
        {
            const auto s = from_lo + Vector2i{i, j};
            const auto d = oct(zone_point(from), L.pt(s));
            dist[L.index(s)] = d;
            arrayAppend(heap, entry{d, L.index(s)});
            std::push_heap(heap.begin(), heap.end(), later);
        }

    while (!heap.isEmpty())
    {
        std::pop_heap(heap.begin(), heap.end(), later);
        const auto [d, i] = heap.back();
        arrayRemoveSuffix(heap);
        if (d > dist[i])
            continue;
        const auto v = Vector2i{(int)(i % (uint32_t)n) + lo, (int)(i / (uint32_t)n) + lo};
        fm_assert(v.x() > lo && v.x() < hi && v.y() > lo && v.y() < hi);

        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
            {
                const auto u = v + Vector2i{dx, dy};
                if (u == v || !L.at(u))
                    continue;
                if (dx && dy && !(L.at(v + Vector2i{dx, 0}) && L.at(v + Vector2i{0, dy})))
                    continue;
                const auto du = d + (dx && dy ? oct.diag : oct.axial);
                if (du < dist[L.index(u)])
                {
                    dist[L.index(u)] = du;
                    arrayAppend(heap, entry{du, L.index(u)});
                    std::push_heap(heap.begin(), heap.end(), later);
                }
            }
    }
}

// The search's goal_radius(): two cells less 2 px per axis. Octile from a node past it can be
// cheaper than any route through the nodes inside it, so it decides the cost.
uint32_t reference_cost(const lattice& L, const Array<uint32_t>& dist, zone to)
{
    const auto oct = Search::octile{(uint32_t)L.step};
    const auto goal_radius = (uint64_t)(2*L.step - 2);
    auto best = unreached;
    for (int j = -L.clear_steps; j <= L.clear_steps; j++)
        for (int i = -L.clear_steps; i <= L.clear_steps; i++)
        {
            const auto v = to.tile*L.tile + Vector2i{i, j};
            const auto d = dist[L.index(v)];
            if (d != unreached && point::distance_squared(L.pt(v), zone_point(to)) <= 2*goal_radius*goal_radius)
                best = Math::min(best, d + oct(L.pt(v), zone_point(to)));
        }
    return best;
}

void check_map(bool for_large_pools, ArrayView<const pool_size> pools)
{
    auto w = world();
    make_map(w, for_large_pools);

    auto A = astar{};
    const auto& pred = Search::without_critters();

    for (auto [cell, bbox] : pools)
    {
        auto pool = Grid::Pass::Pool{Grid::Pass::Params{cell, bbox, pass_mode::pass}, pred};
        fm_assert(pool.params().div_size() == cell);
        pool.maybe_mark_stale_all(w);

        auto L = lattice{(int)cell};
        auto dist = Array<uint32_t>{NoInit, (size_t)(L.n*L.n)};
        for (int y = L.lo; y <= L.hi; y++)
            for (int x = L.lo; x <= L.hi; x++)
                L.free[L.index({x, y})] = A.cache()->is_passable_for_bbox(w, pool, L.pt({x, y}), pred);

        for (auto z : zones)
            for (int j = -L.clear_steps; j <= L.clear_steps; j++)
                for (int i = -L.clear_steps; i <= L.clear_steps; i++)
                    if (!L.at(z.tile*L.tile + Vector2i{i, j}))
                        fm_abort("dijkstra reference: zone %d,%d isn't clear at cell %u, bbox %u",
                                 z.tile.x(), z.tile.y(), cell, bbox);

        for (auto a = 0u; a < array_size(zones); a++)
        {
            reference_costs(L, zones[a], dist);

            for (auto b = 0u; b < array_size(zones); b++)
            {
                if (a == b)
                    continue;
                const auto expected = reference_cost(L, dist, zones[b]);
                const bool found = expected < max_dist;
                const auto r = A.Dijkstra<0, false>(w, pool, zone_point(zones[a]), zone_point(zones[b]),
                                                    max_dist, Vector2ui{bbox}, pred);
                if (r.is_found() != found || (found && r.cost() != expected))
                    fm_abort("dijkstra reference: cell %u, bbox %u, zone %u -> %u: found %d, cost %u;"
                             " reference found %d, cost %u",
                             cell, bbox, a, b, (int)r.is_found(), r.cost(), (int)found, expected);
            }
        }
    }
}

} // namespace

void Test::test_dijkstra_reference()
{
    check_map(false, small_pools);
    check_map(true, large_pools);
}

} // namespace floormat
