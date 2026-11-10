#include "critter-script-cover-movement-range.hpp"
#include "grid-registry.hpp"
#include "point.inl"
#include "search-astar.hpp"
#include "tile-defs.hpp"
#include "world.hpp"
#include "compat/assert.hpp"
#include "compat/floor-divmod.hpp"
#include <algorithm>
#include <functional>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>

namespace floormat::Cover {

movement_range::movement_range()
{
    arrayReserve(points, 16);
    arrayReserve(heap, 16);
}

ArrayView<const point> movement_range::border(world& w, Grid::GridRegistry& pools, point from, Optional<u32> bbox_side,
                                              u32 max_dist)
{
    const auto R = (int)max_dist;
    // The fill reads cells within R + 2 div px of from, div being at most default_div_size, and each cell's grid
    // reads one chunk more.
    const auto pass_gen = w.max_pass_gen(from.chunk3(), (max_dist + 2*astar::default_div_size)/chunk_size_xy + 2);
    if (last_pass_gen == pass_gen && last_from == from && last_bbox_side == bbox_side && last_max_dist == max_dist)
        return points;
    last_pass_gen = pass_gen;
    last_from = from;
    last_bbox_side = bbox_side;
    last_max_dist = max_dist;
    arrayClear(points);
    arrayAppend(points, from);
    if (!R || !bbox_side)
        return points;
    fm_assert(R < (u16)-1);

    auto& pool = pools.pass_pool(Pass::Params{astar::default_div_size, *bbox_side, pass_mode::pass}, Grid::Kind::static_);
    pool.maybe_mark_stale_all(w);
    const auto div = (int)pool.params().div_size(), dc = (int)pool.params().div_count;
    // A cell farther than n from the cell of from lies over R px away.
    const auto n = R/div + 1, side = 2*n + 1;
    const auto count = (u32)(side*side);
    const auto g = Vector3i(from).xy();
    const auto cell_of = [div](int px) { return floor_div(px + half_tile<int>, div); };
    const auto origin = Vector2i{cell_of(g.x()), cell_of(g.y())} - Vector2i{n};
    const auto z = from.chunk3().z;
    if (arrayCapacity(dist) < count)
        arrayReserve(dist, count + count/2);
    if (arrayCapacity(passable) < count)
        arrayReserve(passable, count + count/2);
    arrayResize(dist, NoInit, count);
    arrayResize(passable, NoInit, count);

    Optional<Pass::Grid> grid;
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
                grid = {};
                if (auto* p = w.at(cc))
                {
                    grid.emplace(pool[*p]);
                    grid->build_if_stale();
                }
            }
            const auto local = Vector2ui(cell - ch*dc);
            const auto i = (u32)(y*side + x);
            passable[i] = grid && grid->bit(Pass::Grid::get_bitmask_index(local.x(), local.y(), (u32)dc));
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

    // A ray from inside the reached cells to a body outside them leaves through a border cell, which sees the
    // body as well, to within a cell.
    const auto reached = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < side && y < side && dist[y*side + x] <= R;
    };
    for (auto y = 0; y < side; y++)
        for (auto x = 0; x < side; x++)
        {
            if (!reached(x, y))
                continue;
            bool border = false;
            for (auto dy = -1; dy <= 1; dy++)
                for (auto dx = -1; dx <= 1; dx++)
                    border |= !reached(x + dx, y + dy);
            if (border)
                arrayAppend(points, from + ((origin + Vector2i{x, y})*div + Vector2i{div/2 - half_tile<int>} - g));
        }
    return points;
}

} // namespace floormat::Cover
