#include "search-astar.hpp"
#include "search-constants.hpp"
#include "search-cache.hpp"
#include "grid-pass-pool.hpp"
#include "search-result.hpp"
#include "search.hpp"
#include "world.hpp"
#include "point.inl"
#include "compat/array-size.hpp"
#include "compat/format.hpp"
#include "compat/function2.hpp"
#include <cstdio>
#include <algorithm>
#include <cr/GrowableArray.h>
#include <mg/Functions.h>
#include <mg/Range.h>
#include <mg/Timeline.h>

namespace floormat {

struct astar::visited
{
    uint32_t dist = (uint32_t)-1;
    uint32_t prev = (uint32_t)-1;
    point pt;
};

struct astar::frontier
{
    uint32_t node = (uint32_t)-1;
    uint32_t f_score = (uint32_t)-1;
    uint32_t g_score = (uint32_t)-1;
};

using visited = astar::visited;
using frontier = astar::frontier;
using Search::div_size;
using Search::div_factor;
using Search::min_size;

namespace {

void simplify_path(ArrayView<const point> src, Array<point>& dest)
{
    const auto size = (uint32_t)src.size();

    if (size == 0) [[unlikely]]
        return;

    arrayAppend(dest, src[0]);
    if (size < 2) [[unlikely]]
        return;

    auto last_vec = src[1] - src[0];

    for (auto i = 2u; i < size; i++)
    {
        const auto vec = src[i] - src[i-1];
        if (vec != last_vec)
        {
            if (dest.back() != src[i-1])
                arrayAppend(dest, src[i-1]);
            last_vec = vec;
        }
    }

    if (dest.back() != src.back())
        arrayAppend(dest, src.back());
}

struct dir_step { Vector2i dir; uint32_t len; };

constexpr auto directions = []() constexpr
{
    constexpr auto len1 = div_size;
    constexpr auto len2 = (uint32_t)(len1.length() + 1.f); // NOLINT
    std::array<dir_step, 8> array = {{
        { { -1, -1 }, len2 },
        { {  0, -1 }, len1.y() },
        { {  1, -1 }, len2 },
        { { -1,  0 }, len1.x() },
        { {  1,  1 }, len2 },
        { {  1,  0 }, len1.x() },
        { { -1,  1 }, len2 },
        { {  0,  1 }, len1.y() },
    }};
    for (auto& [vec, len] : array)
        vec *= div_size;
    for (auto i = 0uz; i < array.size(); i++)
        for (auto j = 0uz; j < i; j++)
            fm_assert(array[i].dir != array[j].dir);
    return array;
}();

struct heap_comparator
{
    static bool operator()(frontier a, frontier b)
    {
        const auto ka = (uint64_t{a.f_score} << 32) | uint64_t{~a.g_score};
        const auto kb = (uint64_t{b.f_score} << 32) | uint64_t{~b.g_score};
        return ka > kb;
    }
};

void set_result_from_idx(path_search_result& result,
                         Array<point>& temp_nodes, const Array<visited>& nodes,
                         point from, point to, const uint32_t idx)
{
    uint32_t len = 0;
    for (auto i = idx; i != (uint32_t)-1; i = nodes[i].prev)
        len++;

    if (!len) [[unlikely]]
        return;

    fm_debug_assert(idx != (uint32_t)-1);

    const auto& to_node = nodes[idx];
    result.set_cost(to_node.dist + point::distance(to, to_node.pt));

    arrayClear(temp_nodes);
    arrayReserve(temp_nodes, len + 2);
    arrayClear(result.raw_path());
    arrayReserve(result.raw_path(), len + 2);

    for (auto i = idx; i != (uint32_t)-1; i = nodes[i].prev)
        arrayAppend(temp_nodes, nodes[i].pt);
    arrayAppend(temp_nodes, from);

    std::reverse(temp_nodes.begin(), temp_nodes.end());
    simplify_path(temp_nodes, result.raw_path());
    arrayClear(temp_nodes);
}

void add_to_heap(Array<frontier>& Q, uint32_t id, uint32_t f_score, uint32_t g_score)
{
    arrayAppend(Q, frontier { .node = id, .f_score = f_score, .g_score = g_score, });
    std::push_heap(Q.begin(), Q.end(), heap_comparator{});
}

frontier pop_from_heap(Array<frontier>& Q)
{
    std::pop_heap(Q.begin(), Q.end(), heap_comparator{});
    const auto n = Q.back();
    arrayRemoveSuffix(Q);
    return n;
}

bool is_passable_swept(world& w, Search::cache& cache, Grid::Pass::Pool& pool,
                       point a, point b, const astar::pred& p)
{
    constexpr int div = (int)div_size.x();
    constexpr int half_tile = tile_size_xy / 2;

    const Vector2i a_pix = iTILE_SIZE2 * Vector2i(a.local()) + Vector2i(a.offset());
    const Vector2i b_pix = a_pix + (b - a);
    const Vector2i lo_pix = Math::min(a_pix, b_pix);
    const Vector2i hi_pix = Math::max(a_pix, b_pix);

    constexpr auto floor_div = [](int s) constexpr -> int
    {
        const int q = s / div;
        return (s < 0 && s % div) ? q - 1 : q;
    };

    const int idx_x_lo = floor_div(lo_pix.x() + half_tile);
    const int idx_x_hi = floor_div(hi_pix.x() + half_tile);
    const int idx_y_lo = floor_div(lo_pix.y() + half_tile);
    const int idx_y_hi = floor_div(hi_pix.y() + half_tile);

    for (int iy = idx_y_lo; iy <= idx_y_hi; iy++)
        for (int ix = idx_x_lo; ix <= idx_x_hi; ix++)
        {
            const Vector2i cell_pix{ix * div - half_tile + div/2,
                                    iy * div - half_tile + div/2};
            const auto pt_in_cell = point::normalize_coords(a, cell_pix - a_pix);
            if (!cache.is_passable_for_bbox(w, pool, pt_in_cell, p))
                return false;
        }
    return true;
}

template<bool IsDiagonal, int Debug>
CORRADE_ALWAYS_INLINE
void do_dir(world& w, Grid::Pass::Pool& pool, Search::cache& cache,
            Array<visited>& nodes, Array<frontier>& Q,
            const astar::pred& p, const astar::heuristic& h,
            dir_step dirs,
            point to, point cur_pt,
            uint32_t cur_idx, uint32_t cur_dist,
            uint32_t max_dist)
{
    const auto [vec, len] = dirs;

    const auto dist = cur_dist + len;
    const auto new_pt = point::normalize_coords(cur_pt, vec);
    uint32_t f_score = dist + h(new_pt, to);
    if (f_score >= max_dist) [[unlikely]]
        return;

    auto chunk_idx = cache.get_chunk_index(Vector2i(new_pt.chunk()));
    auto tile_idx = cache.get_tile_index(new_pt.local(), new_pt.offset());
    auto new_idx = cache.lookup_index(chunk_idx, tile_idx);

    if (new_idx != (uint32_t)-1)
        if (nodes[new_idx].dist <= dist)
            return;

    if (IsDiagonal ? !cache.is_passable_between_diag(w, pool, cur_pt, new_pt, p)
                   : !cache.is_passable_for_bbox(w, pool, new_pt, p))
        return;

    if (new_idx == (uint32_t)-1)
    {
        const auto sz = nodes.size();
        new_idx = (uint32_t)sz;
        cache.add_index(chunk_idx, tile_idx, new_idx);
        auto new_node = visited{
            .dist = dist,
            .prev = cur_idx,
            .pt = new_pt,
        };
        arrayAppend(nodes, new_node);
    }
    else
    {
        auto& n = nodes[new_idx];
        n.dist = dist;
        n.prev = cur_idx;
    }

    if constexpr (Debug >= 3) [[unlikely]]
        DBG_nospace << " path:" << dist
        << " pos:" << Vector3i(new_pt.coord())
        << ";" << new_pt.offset();

    add_to_heap(Q, new_idx, f_score, dist);
};

} // namespace

astar::astar() :
    _cache{InPlaceInit, (uint32_t)Search::div_size.x()}
{
}

astar::~astar() noexcept = default;

void astar::reserve(size_t capacity)
{
    arrayReserve(nodes, capacity);
    arrayReserve(Q, capacity);
}

void astar::clear()
{
    arrayResize(nodes, 0);
    arrayResize(Q, 0);
}

Search::cache* astar::cache() { return &*_cache; }

template<int Debug>
path_search_result astar::Dijkstra(world& w, const point from, const point to,
                                   uint32_t max_dist, Vector2ui own_size_,
                                   const pred& p, const heuristic& h)
{
    reserve(initial_capacity);

    Timeline timeline;
    if constexpr(Debug > 0) [[unlikely]]
        timeline.start();

    clear();
    auto& cache = *_cache;
    cache.allocate(from, max_dist);

    constexpr auto size_max = uint32_t{tile_size_xy}*uint32_t{TILE_MAX_DIM};
    fm_assert(own_size_ < Vector2ui{size_max});
    const auto own_size = Math::max(own_size_, min_size);
    constexpr auto goal_thres_lin = (uint32_t)(div_size.length() + 1.5f);

    if (from.coord().z() != to.coord().z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.coord().z() != 0) [[unlikely]]
        return {};

    const auto bbox_size = Math::max(own_size.x(), own_size.y());
    auto& pool = w.pass_pool_registry().pool_for(bbox_size);
    pool.maybe_mark_stale_all(w.frame_no());

    if (auto R = Range2D::fromCenter(TILE_SIZE2 * Vector2(from.local()) + Vector2(from.offset()), Vector2(own_size/2));
        !path_search::is_passable_(w.chunk_at_memo(from.chunk3()), w.neighbors(from.chunk3()), R.min(), R.max(), p))
        return {};

    if (auto R = Range2D::fromCenter(TILE_SIZE2 * Vector2(to.local()) + Vector2(to.offset()), Vector2(own_size/2));
        !path_search::is_passable_(w.chunk_at_memo(to.chunk3()), w.neighbors(to.chunk3()), R.min(), R.max(), p))
        return {};

    constexpr Vector2i seed_offsets[9] = {
        {  0,             0            },
        {  0,            -div_size.y() },
        { -div_size.x(),  0            },
        {  div_size.x(),  0            },
        {  0,             div_size.y() },
        { -div_size.x(), -div_size.y() },
        {  div_size.x(), -div_size.y() },
        { -div_size.x(),  div_size.y() },
        {  div_size.x(),  div_size.y() },
    };

    for (auto off : seed_offsets)
    {
        auto pt = point::normalize_coords({from.coord(), {}}, off);
        auto dist = h(from, pt);

        if (cache.is_passable_for_bbox(w, pool, pt, p))
        {
            auto idx = (uint32_t)nodes.size();
            cache.add_index(pt, idx);
            arrayAppend(nodes, {.dist = dist, .prev = (uint32_t)-1, .pt = pt, });
            uint32_t f_score = dist + h(pt, to);
            if (dist < max_dist && f_score < max_dist)
                add_to_heap(Q, idx, f_score, dist);
        }
    }

    auto closest_h = (uint32_t)-1;
    uint32_t closest_idx = (uint32_t)-1;
    auto goal_idx = (uint32_t)-1;
    auto to_idx = (uint32_t)-1;

    while (!Q.isEmpty())
    {
        const auto front = pop_from_heap(Q);
        const auto cur_idx = front.node;
        point cur_pt;
        uint32_t cur_dist;
        auto& n = nodes[cur_idx];
        if (front.g_score > n.dist)
            continue;
        cur_pt = n.pt;
        cur_dist = n.dist;

        if (cur_idx == to_idx) [[unlikely]]
        {
            goal_idx = cur_idx;
            break;
        }

        const uint32_t goal_dist = point::distance(cur_pt, to);

        if (goal_dist < closest_h)
        {
            closest_h = goal_dist;
            closest_idx = cur_idx;

            if constexpr (Debug >= 2) [[unlikely]]
                DBG_nospace << "closest node"
                            << " px:" << closest_h << " path:" << cur_dist
                            << " pos:" << cur_pt;
        }

        if (goal_dist < goal_thres_lin) [[unlikely]]
        {
            if (is_passable_swept(w, cache, pool, cur_pt, to, p))
            {
                const auto new_dist = cur_dist + goal_dist;
                if (to_idx == (uint32_t)-1)
                {
                    to_idx = (uint32_t)nodes.size();
                    arrayAppend(nodes, visited{ .dist = new_dist, .prev = cur_idx, .pt = to, });
                    add_to_heap(Q, to_idx, new_dist, new_dist);
                }
                else if (new_dist < nodes[to_idx].dist)
                {
                    auto& tn = nodes[to_idx];
                    tn.dist = new_dist;
                    tn.prev = cur_idx;
                    add_to_heap(Q, to_idx, new_dist, new_dist);
                }
            }
        }

        for (auto i = 0u; i < 8; i += 2)
        {
            const auto d1 = directions.data()[i + 0],
                       d2 = directions.data()[i + 1];
            do_dir<1, Debug>(w, pool, cache, nodes, Q, p, h, d1, to, cur_pt, cur_idx, cur_dist, max_dist);
            do_dir<0, Debug>(w, pool, cache, nodes, Q, p, h, d2, to, cur_pt, cur_idx, cur_dist, max_dist);
        }
    }

    path_search_result result;

    if (goal_idx != (uint32_t)-1)
    {
        result.set_found(true);
        result.set_distance(0);
        set_result_from_idx(result, temp_nodes, nodes, from, to, goal_idx);
    }
    else if (closest_idx != (uint32_t)-1)
    {
        result.set_found(false);
        result.set_distance(closest_h);
        set_result_from_idx(result, temp_nodes, nodes, from, to, closest_idx);
    }

    result.set_time(timeline.currentFrameTime());

    if constexpr (Debug >= 1) [[unlikely]]
    {
        auto d0_ =
            Vector2i(Math::abs(from.coord() - to.coord())) * iTILE_SIZE2
            + Vector2i(Math::abs(Vector2i(from.offset()) - Vector2i(to.offset())));
        auto d0 = (uint32_t)d0_.length();
        char buf[128];
        size_t len = 0;
        const auto time = result.time() * 1e3f;
        if (goal_idx != (uint32_t)-1)
        {
            auto d = nodes[goal_idx].dist;
            len = snformat(buf, "Dijkstra: found in {:.2f} ms "
                                "len:{} len0:{} ratio:{:.4}\n"_cf,
                           time, d, d0,
                           d > 0 && d0 > 0 ? (float)d/(float)d0 : 1);
        }
        else if (closest_idx != (uint32_t)-1)
        {
            const auto& closest = nodes[closest_idx];
            fm_assert(closest.dist != 0 && closest.dist != (uint32_t)-1);
            len = snformat(buf, "Dijkstra: no path found in {:.2f} ms "
                                "closest:{} len:{} len0:{} ratio:{:.4}\n"_cf,
                           time, closest_h, closest.dist, d0,
                           d0 > 0 ? (float)closest.dist/(float)d0 : 1);
        }
        if (len)
        {
            len = Math::min(len, array_size(buf)-1);
            std::fwrite(buf, len, 1, stdout);
            std::fflush(stdout);
        }
    }

    arrayResize(Q, 0);

    return result;
}

template path_search_result astar::Dijkstra<0>(world&, point, point, uint32_t, Vector2ui, const pred&, const heuristic&);
template path_search_result astar::Dijkstra<1>(world&, point, point, uint32_t, Vector2ui, const pred&, const heuristic&);
template path_search_result astar::Dijkstra<2>(world&, point, point, uint32_t, Vector2ui, const pred&, const heuristic&);
template path_search_result astar::Dijkstra<3>(world&, point, point, uint32_t, Vector2ui, const pred&, const heuristic&);

} // namespace floormat
