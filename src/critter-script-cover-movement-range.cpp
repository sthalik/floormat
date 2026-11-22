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

// border() puts r/32 - 1 points between the corners on each side of its octagon of radius r, about 24 px apart.
constexpr inline int octagon_radius_per_side_point = 32;

} // namespace

bool movement_range::cache_key::operator==(const cache_key&) const = default;

movement_range::movement_range()
{
    arrayReserve(cached_points, 16);
    arrayReserve(heap, 16);
    arrayReserve(border_cell_offsets, 16);
}

ArrayView<const point> movement_range::border(world& w, Grid::GridRegistry& pools, point from,
                                              std::array<object_id, 2> excluded_blocker_ids,
                                              Optional<u32> bbox_side, u32 max_dist)
{
    const auto R = (int)max_dist;
    // The Dijkstra search reads cells within R + 2 div px of from, div being at most default_div_size, and each
    // cell's grid reads one chunk more.
    const auto collider_gen =
        w.max_collider_gen(from.chunk3(), (max_dist + 2*astar::default_div_size)/chunk_size_xy + 2);
    const cache_key key{from, excluded_blocker_ids, bbox_side, 0, 0, max_dist, collider_gen};
    if (_key == key)
        return cached_points;
    _key = key;
    arrayClear(cached_points);
    arrayAppend(cached_points, from);
    if (!R || !bbox_side)
        return cached_points;
    fm_assert(R < (u16)-1);

    auto& objects = pools.object_pool(Pass::Params{astar::default_div_size, *bbox_side, pass_mode::pass});
    const auto div = (int)objects.params().div_size();
    // A cell farther than n from the cell of from lies over R px away.
    const auto n = R/div + 1, side = 2*n + 1;
    const auto g = Vector3i(from).xy();
    const auto origin = compute_dijkstra_map(w, objects, from, excluded_blocker_ids, n, R);

    for (const auto r : { R, R*3/4, R/2 })
    {
        const auto is_reached = [&](int x, int y) {
            return x >= 0 && y >= 0 && x < side && y < side && dist[y*side + x] <= r;
        };
        arrayClear(border_cell_offsets);
        for (auto y = 0; y < side; y++)
            for (auto x = 0; x < side; x++)
            {
                if (!is_reached(x, y))
                    continue;
                bool border = false;
                for (auto dy = -1; dy <= 1; dy++)
                    for (auto dx = -1; dx <= 1; dx++)
                        border |= !is_reached(x + dx, y + dy);
                if (border)
                    arrayAppend(border_cell_offsets,
                                (origin + Vector2i{x, y})*div + Vector2i{div/2 - half_tile<int>} - g);
            }
        if (border_cell_offsets.isEmpty())
            continue;
        // Centered on from's cell, so the points stay the same while `from` moves within that cell.
        const auto center = (origin + Vector2i{n})*div + Vector2i{div/2 - half_tile<int>} - g;
        const auto d = (int)(r*Math::sqrt(.5));
        const Vector2i corners[] = { {r, 0}, {d, d}, {0, r}, {-d, d}, {-r, 0}, {-d, -d}, {0, -r}, {d, -d} };
        const auto num_points_between_corners = Math::max(r/octagon_radius_per_side_point - 1, 0);
        for (auto j = 0u; j < 8; j++)
            for (auto k = 0; k <= num_points_between_corners; k++)
            {
                const auto a = corners[j], b = corners[(j + 1) % 8];
                const auto p = center + a + (b - a)*k/(num_points_between_corners + 1);
                // Among blockers an octagon point may lie outside the area, so it takes the nearest edge point.
                auto nearest = border_cell_offsets[0];
                for (const auto& off : border_cell_offsets.exceptPrefix(1))
                    if ((off - p).dot() < (nearest - p).dot())
                        nearest = off;
                const auto pt = from + nearest;
                if (std::find(cached_points.begin(), cached_points.end(), pt) == cached_points.end())
                    arrayAppend(cached_points, pt);
            }
    }
    return cached_points;
}

ArrayView<const point> movement_range::reachable_lattice_points(world& w, Grid::GridRegistry& pools, point from,
                                                                std::array<object_id, 2> excluded_blocker_ids,
                                                                Optional<u32> bbox_side, u32 radius, u32 spacing,
                                                                u32 max_walk_dist)
{
    fm_assert(spacing);
    const auto collider_gen =
        w.max_collider_gen(from.chunk3(), (radius + 2*astar::default_div_size)/chunk_size_xy + 2);
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
    const auto origin = compute_dijkstra_map(w, objects, from, excluded_blocker_ids, n,
                                             (int)Math::min(max_walk_dist, (u32)(u16)-2));
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

Vector2i movement_range::compute_dijkstra_map(world& w, Grid::Object::Pool& objects, point from,
                                              std::array<object_id, 2> excluded_blocker_ids, int n, int max_dist)
{
    auto& pool = objects.pass_pool();
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
                    auto object_grid = objects[*p];
                    object_grid.build_if_stale();
                    if (object_grid.blocked_objects(pred, entry_blocked))
                        object_gridʹ = object_grid;
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
