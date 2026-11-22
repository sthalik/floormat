#include "critter-script-cover-movement-range.hpp"
#include "grid-object.hpp"
#include "grid-registry.hpp"
#include "point.inl"
#include "search-astar.hpp"
#include "tile-defs.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include "compat/floor-divmod.hpp"
#include "compat/function-view.hpp"
#include <algorithm>
#include <functional>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>

namespace floormat::Cover {

namespace {

int cell_of(int px, int div) { return floor_div(px + half_tile<int>, div); }

// Px along a border between two points border() returns.
constexpr inline int border_point_average_spacing = 24;

// Clockwise with y pointing down, starting north.
constexpr inline Vector2i neighbor_offsets[] = {
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
};
constexpr inline u32 east = 2, west = 6;

// Calls emit(cell) for cells along each border of the cells labelled 1 in the side² labels, starting with each
// border's first cell. Borders come from Suzuki and Abe's border following, and labels then hold its f.
template<typename F>
void trace_borders(ArrayView<u8> labels, int side, int div, int diag, F&& emit)
{
    // Without the border hierarchy only f's sign matters, so every border gets the same NBD.
    constexpr u8 minus_nbd = 2, nbd = 3;
    const auto label = [&](Vector2i p) -> u8 {
        return p.x() >= 0 && p.y() >= 0 && p.x() < side && p.y() < side ? labels[p.y()*side + p.x()] : 0;
    };
    const auto set_label = [&](Vector2i p, u8 f) { labels[p.y()*side + p.x()] = f; };
    for (auto y = 0; y < side; y++)
        for (auto x = 0; x < side; x++)
        {
            const auto start = Vector2i{x, y};
            const auto f = label(start);
            u32 back;
            if (f == 1 && !label(start + neighbor_offsets[west]))
                back = west;
            else if ((f == 1 || f == nbd) && !label(start + neighbor_offsets[east]))
                back = east;
            else
                continue;
            emit(start);
            auto k = 1u;
            while (k < 8 && !label(start + neighbor_offsets[(back + k) % 8]))
                k++;
            if (k == 8)
            {
                set_label(start, minus_nbd);
                continue;
            }
            back = (back + k) % 8;
            const auto second = start + neighbor_offsets[back];
            auto cur = start;
            auto len = 0;
            for (;;)
            {
                auto d = (back + 7) % 8;
                auto is_east_examined = false;
                while (!label(cur + neighbor_offsets[d]))
                {
                    is_east_examined |= d == east;
                    d = (d + 7) % 8;
                }
                const auto next = cur + neighbor_offsets[d];
                if (is_east_examined)
                    set_label(cur, minus_nbd);
                else if (label(cur) == 1)
                    set_label(cur, nbd);
                if (next == start && cur == second)
                    break;
                // The remainder carries over, so the average holds on any border.
                len += d % 2 ? diag : div;
                if (len >= border_point_average_spacing)
                {
                    len -= border_point_average_spacing;
                    emit(next);
                }
                cur = next;
                back = (d + 4) % 8;
            }
        }
}

} // namespace

bool movement_range::cache_key::operator==(const cache_key&) const = default;

movement_range::movement_range()
{
    arrayReserve(cached_points, 16);
    arrayReserve(heap, 16);
}

ArrayView<const point> movement_range::border(world& w, Grid::GridRegistry& pools, point from,
                                              Optional<u32> bbox_side, u32 max_dist)
{
    const auto R = (int)max_dist;
    // The Dijkstra search reads cells within 9/8 R + 2 div px of from, div being at most default_div_size, and
    // each cell's grid reads one chunk more.
    const auto max_pass_gen =
        w.max_pass_gen(from.chunk3(), (max_dist*9/8 + 2*astar::default_div_size)/chunk_size_xy + 2);
    const cache_key key{from, {}, bbox_side, 0, 0, max_dist, max_pass_gen};
    if (_key == key)
        return cached_points;
    _key = key;
    arrayClear(cached_points);
    arrayAppend(cached_points, from);
    if (!R || !bbox_side)
        return cached_points;

    auto& pool = pools.pass_pool(Pass::Params{astar::default_div_size, *bbox_side, pass_mode::pass});
    const auto div = (int)pool.params().div_size();
    const auto diag = (int)(div*Math::sqrt(2.) + .5);
    // In open ground a cell's Dijkstra distance is at most max_ratio times its straight-line distance.
    const auto max_ratio = Math::sqrt(1 + Math::pow((diag - div)/(double)div, 2.));
    fm_debug_assert(max_ratio < 9./8);
    const auto max_walk_dist = (int)Math::ceil(R*max_ratio);
    fm_assert(max_walk_dist < (u16)-1);
    // A cell farther than n from the cell of from lies over max_walk_dist px away.
    const auto n = max_walk_dist/div + 1, side = 2*n + 1;
    const auto count = (u32)(side*side);
    const auto g = Vector3i(from).xy();
    const auto origin = compute_dijkstra_map(w, pool, nullptr, from, {}, n, max_walk_dist);
    if (arrayCapacity(cell_labels) < count)
        arrayReserve(cell_labels, count + count/2);
    arrayResize(cell_labels, NoInit, count);

    for (const auto r : { R, R*3/4, R/2 })
    {
        // Measured from the center of from's cell, so the points stay the same while `from` moves within it.
        const auto max_walk_dist_r = (int)Math::ceil(r*max_ratio);
        for (auto y = 0; y < side; y++)
            for (auto x = 0; x < side; x++)
            {
                const auto i = (u32)(y*side + x);
                const auto dx = (i64)(x - n)*div, dy = (i64)(y - n)*div;
                cell_labels[i] = dist[i] <= max_walk_dist_r && dx*dx + dy*dy <= (i64)r*r;
            }
        trace_borders(cell_labels, side, div, diag, [&](Vector2i cell) {
            const auto pt = from + ((origin + cell)*div + Vector2i{div/2 - half_tile<int>} - g);
            if (std::find(cached_points.begin(), cached_points.end(), pt) == cached_points.end())
                arrayAppend(cached_points, pt);
        });
    }
    return cached_points;
}

ArrayView<const point> movement_range::reachable_lattice_points(world& w, Grid::GridRegistry& pools, point from,
                                                                std::array<object_id, 2> excluded_blocker_ids,
                                                                Optional<u32> bbox_side, u32 radius, u32 spacing,
                                                                u32 max_walk_dist)
{
    return reachable_lattice_points_impl(w, pools, from, excluded_blocker_ids, bbox_side, radius, spacing,
                                         max_walk_dist);
}

ArrayView<const point> movement_range::reachable_lattice_points_without_critters(
    world& w, Grid::GridRegistry& pools, point from, Optional<u32> bbox_side, u32 radius, u32 spacing,
    u32 max_walk_dist)
{
    return reachable_lattice_points_impl(w, pools, from, {}, bbox_side, radius, spacing, max_walk_dist);
}

ArrayView<const point> movement_range::reachable_lattice_points_impl(
    world& w, Grid::GridRegistry& pools, point from, Optional<std::array<object_id, 2>> excluded_blocker_ids,
    Optional<u32> bbox_side, u32 radius, u32 spacing, u32 max_walk_dist)
{
    fm_assert(spacing);
    const auto chunk_radius = (radius + 2*astar::default_div_size)/chunk_size_xy + 2;
    const auto collider_gen = excluded_blocker_ids ? w.max_collider_gen(from.chunk3(), chunk_radius)
                                                   : w.max_pass_gen(from.chunk3(), chunk_radius);
    const cache_key key{from, excluded_blocker_ids, bbox_side, radius, spacing, max_walk_dist, collider_gen};
    if (_key == key)
        return cached_points;
    _key = key;
    arrayClear(cached_points);
    arrayAppend(cached_points, from);
    if (!radius || !bbox_side)
        return cached_points;

    auto& objects = pools.object_pool(Pass::Params{astar::default_div_size, *bbox_side, pass_mode::pass});
    const auto div = (int)objects.params().div_size();
    const auto n = ((int)radius + div - 1)/div, side = 2*n + 1;
    // No path inside the window is longer than one diagonal step per cell, so the Dijkstra search isn't cut short.
    fm_assert(side*side*2*div < (u16)-1);
    const auto max_dist = (int)Math::min(max_walk_dist, (u32)(u16)-2);
    const auto origin =
        excluded_blocker_ids
            ? compute_dijkstra_map(w, objects.pass_pool(), &objects, from, *excluded_blocker_ids, n, max_dist)
            : compute_dijkstra_map(w, objects.pass_pool(), nullptr, from, {}, n, max_dist);
    const auto g = Vector3i(from).xy();
    const auto k = (int)(radius/spacing);
    for (auto j = -k; j <= k; j++)
        for (auto i = -k; i <= k; i++)
        {
            if (!i && !j)
                continue;
            const auto d = Vector2i{i, j}*(int)spacing;
            const auto pos = g + d;
            const auto cell = Vector2i{cell_of(pos.x(), div), cell_of(pos.y(), div)} - origin;
            fm_debug_assert(cell.x() >= 0 && cell.y() >= 0 && cell.x() < side && cell.y() < side);
            if (dist[cell.y()*side + cell.x()] != (u16)-1)
                arrayAppend(cached_points, from + d);
        }
    return cached_points;
}

Vector2i movement_range::compute_dijkstra_map(world& w, Grid::Pass::Pool& pool, Grid::Object::Pool* objects,
                                              point from, std::array<object_id, 2> excluded_blocker_ids, int n,
                                              int max_dist)
{
    const auto div = (int)pool.params().div_size(), dc = (int)pool.params().div_count;
    const auto R = max_dist;
    const auto side = 2*n + 1;
    const auto count = (u32)(side*side);
    const auto g = Vector3i(from).xy();
    const auto origin = Vector2i{cell_of(g.x(), div), cell_of(g.y(), div)} - Vector2i{n};
    const auto z = from.chunk3().z;
    if (arrayCapacity(dist) < count)
        arrayReserve(dist, count + count/2);
    if (arrayCapacity(passable) < count)
        arrayReserve(passable, count + count/2);
    arrayResize(dist, NoInit, count);
    arrayResize(passable, NoInit, count);

    // TODO: much later, factions may decide which critters block, and a player standing in a doorway may be
    // pushed away.
    const auto pred = Search::blocks_all_but(excluded_blocker_ids[0], excluded_blocker_ids[1]);
    Optional<Pass::Grid> gridʹ;
    // grid_ch's object grid while one of its objects blocks, with entry_blocked filled for it.
    Optional<Grid::Object::Grid> object_gridʹ;
    chunk_coords_ grid_ch;
    bool looked = false;
    for (auto y = 0; y < side; y++)
        for (auto x = 0; x < side; x++)
        {
            const auto cell = origin + Vector2i{x, y};
            const auto ch = Vector2i{floor_div(cell.x(), dc), floor_div(cell.y(), dc)};
            const auto cc = chunk_coords_{(i16)ch.x(), (i16)ch.y(), z};
            if (!looked || cc != grid_ch)
            {
                looked = true;
                grid_ch = cc;
                gridʹ = {};
                object_gridʹ = {};
                if (auto* p = w.at(cc))
                {
                    gridʹ.emplace(pool[*p]).build_if_stale();
                    if (objects)
                    {
                        auto object_grid = (*objects)[*p];
                        object_grid.build_if_stale();
                        if (object_grid.blocked_objects(pred, entry_blocked))
                            object_gridʹ = object_grid;
                    }
                }
            }
            const auto local = Vector2ui(cell - ch*dc);
            const auto i = (u32)(y*side + x);
            passable[i] = false;
            if (gridʹ)
            {
                const auto& grid = *gridʹ;
                const auto k = Pass::Grid::get_bitmask_index(local.x(), local.y(), (u32)dc);
                passable[i] = grid.bit(k);
                if (passable[i] && object_gridʹ)
                {
                    const auto& object_grid = *object_gridʹ;
                    passable[i] = object_grid.uses_u32() ? !object_grid.is_blocked<u32>(k, entry_blocked)
                                                         : !object_grid.is_blocked<u16>(k, entry_blocked);
                }
            }
            dist[i] = (u16)-1;
        }

    const auto diag = (int)(div*Math::sqrt(2.) + .5);
    const auto open = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < side && y < side && passable[y*side + x];
    };
    arrayClear(heap);
    // A bit is coarser than a point, so the cell of from is reached whatever its bit.
    dist[n*side + n] = 0;
    arrayAppend(heap, (u64)(n*side + n));
    while (!heap.isEmpty())
    {
        std::pop_heap(heap.begin(), heap.end(), std::greater<>{});
        const auto key = heap.back();
        arrayRemoveSuffix(heap, 1);
        const auto d = (int)(key >> 32), i = (int)(u32)key;
        if (d > dist[i])
            continue;
        const auto x = i % side, y = i / side;
        for (auto dy = -1; dy <= 1; dy++)
            for (auto dx = -1; dx <= 1; dx++)
            {
                if (!dx && !dy || !open(x + dx, y + dy))
                    continue;
                // A diagonal step needs both cells beside it, as the body would clip their corners.
                if (dx && dy && (!open(x + dx, y) || !open(x, y + dy)))
                    continue;
                const auto nd = d + (dx && dy ? diag : div);
                const auto j = (y + dy)*side + x + dx;
                if (nd > R || nd >= dist[j])
                    continue;
                dist[j] = (u16)nd;
                arrayAppend(heap, (u64)nd << 32 | (u32)j);
                std::push_heap(heap.begin(), heap.end(), std::greater<>{});
            }
    }
    return origin;
}

} // namespace floormat::Cover
