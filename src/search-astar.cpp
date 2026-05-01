#include "search-astar.hpp"
#include "search-constants.hpp"
#include "search-cache.hpp"
#include "search-pool.hpp"
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
    arrayClear(dest);
    arrayReserve(dest, size);

    if (size < 2) [[unlikely]] // Safely handle 0 or 1 element arrays
    {
        if (size == 1)
            arrayAppend(dest, src[0]);
        return;
    }

    auto last_vec = src[1] - src[0];

    if (last_vec.isZero()) [[unlikely]]
    {
        fm_assert(size <= 2);
        return;
    }

    arrayAppend(dest, src[1]);

    for (auto i = 2u; i < size; i++)
    {
        const auto pos = src[i];
        const auto vec = pos - src[i-1];

        if (vec != last_vec)
        {
            arrayAppend(dest, src[i-1]);
            last_vec = vec;
        }
    }

    if (dest.back() != src.back())
        arrayAppend(dest, src.back());
}

constexpr auto directions = []() constexpr
{
    struct pair { Vector2i dir; uint32_t len; };
    constexpr auto len1 = div_size;
    constexpr auto len2 = (uint32_t)(len1.length() + 1.f); // NOLINT
    std::array<pair, 8> array = {{
        { { -1, -1 }, len2 },
        { {  0, -1 }, len1.y() },
        { {  1, -1 }, len2 },
        { { -1,  0 }, len1.x() },
        { {  1,  0 }, len1.x() },
        { { -1,  1 }, len2 },
        { {  0,  1 }, len1.y() },
        { {  1,  1 }, len2 },
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
        if (a.f_score == b.f_score) [[unlikely]]
            return a.g_score < b.g_score;
        else
            return a.f_score > b.f_score;
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
    arrayResize(temp_nodes, 0);
    arrayReserve(temp_nodes, len+1);

    const auto& to_node = nodes[idx];
    if (result.is_found() && to != to_node.pt)
        arrayAppend(temp_nodes, to);
    result.set_cost(to_node.dist);

    auto i = idx;
    do {
        const auto& node = nodes[i];
        arrayAppend(temp_nodes, node.pt);
        i = node.prev;
    } while (i != (uint32_t)-1);

    if (temp_nodes.back() != from)
        arrayAppend(temp_nodes, from);

    std::reverse(temp_nodes.begin(), temp_nodes.end());
    simplify_path(temp_nodes, result.raw_path());
    arrayResize(temp_nodes, 0);
}

uint32_t distance_sq(point a, point b)
{
    Vector2i dist = a - b;
    return (uint32_t)Vector2(dist).dot();
}

} // namespace

astar::astar() :
    _cache{InPlaceInit, (uint32_t)Search::div_size.x()},
    _registry{InPlaceInit, (uint32_t)Search::div_size.x()}
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

void astar::add_to_heap(uint32_t id, uint32_t f_score, uint32_t g_score)
{
    arrayAppend(Q, frontier { .node = id, .f_score = f_score, .g_score = g_score, });
    std::push_heap(Q.begin(), Q.end(), heap_comparator{});
}

frontier astar::pop_from_heap()
{
    std::pop_heap(Q.begin(), Q.end(), heap_comparator{});
    const auto n = Q.back();
    arrayRemoveSuffix(Q);
    return n;
}

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
    constexpr auto goal_thres_lin = div_size.length() + 1.5f;
    constexpr auto goal_thres = (uint32_t)(goal_thres_lin * goal_thres_lin);

    if (from.coord().z() != to.coord().z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.coord().z() != 0) [[unlikely]]
        return {};

    const auto bbox_size = Math::max(own_size.x(), own_size.y());
    auto& pool = _registry->pool_for(bbox_size);
    pool.maybe_mark_stale_all(w.frame_no(), cache);

    if (!cache.is_passable_for_bbox(w, pool, from, p))
        return {};

    if (!cache.is_passable_for_bbox(w, pool, to, p))
        return {};

    constexpr Vector2i seed_offsets[4] = {
        { 0, -div_size.y() },
        { -div_size.x(),  0 },
        {  div_size.x(),  0 },
        { 0,  div_size.y() },
    };

    for (auto off : seed_offsets)
    {
        constexpr auto min_dist = (uint32_t)((TILE_SIZE2*2.f).length() + 1.f);
        auto pt = point::normalize_coords({from.coord(), {}}, off);
        auto dist = h(from, pt) + min_dist;

        if (cache.is_passable_for_bbox(w, pool, pt, p))
        {
            auto idx = (uint32_t)nodes.size();
            cache.add_index(pt, idx);
            arrayAppend(nodes, {.dist = dist, .prev = (uint32_t)-1, .pt = pt, });
            uint32_t f_score = dist + h(pt, to);
            add_to_heap(idx, f_score, dist);
        }
    }

    auto closest_dist = (uint32_t)-1;
    uint32_t closest_idx = (uint32_t)-1;
    auto goal_idx = (uint32_t)-1;

    while (!Q.isEmpty())
    {
        const auto front = pop_from_heap();
        const auto cur_idx = front.node;
        point cur_pt;
        uint32_t cur_dist;
        auto& n = nodes[cur_idx];
        if (front.g_score > n.dist)
            continue;
        cur_pt = n.pt;
        cur_dist = n.dist;

        if (cur_dist >= max_dist) [[unlikely]]
            continue;

        const auto d = distance_sq(cur_pt, to);

        if (d < closest_dist) [[unlikely]]
        {
            closest_dist = d;
            closest_idx = cur_idx;

            if constexpr (Debug >= 2) [[unlikely]]
                DBG_nospace << "closest node"
                            << " px:" << Math::sqrt(closest_dist) << " path:" << cur_dist
                            << " pos:" << cur_pt;
        }

        if (d < goal_thres) [[unlikely]]
        {
            if (cache.is_passable_between(w, pool, cur_pt, to, p))
            {
                goal_idx = cur_idx;
                break; // path can only get longer
            }
        }

        for (auto [vec, len] : directions)
        {
            const auto dist = cur_dist + len;
            if (dist >= max_dist)
                continue;

            const auto new_pt = point::normalize_coords(cur_pt, vec);

            uint32_t f_score = dist + h(new_pt, to);
            if (f_score >= max_dist)
                continue;

            auto chunk_idx = cache.get_chunk_index(Vector2i(new_pt.chunk()));
            auto tile_idx = cache.get_tile_index(new_pt.local(), new_pt.offset());
            auto new_idx = cache.lookup_index(chunk_idx, tile_idx);

            if (new_idx != (uint32_t)-1)
            {
                if (nodes[new_idx].dist <= dist)
                    continue;
            }

            if (!cache.is_passable_between(w, pool, cur_pt, new_pt, p))
                continue;

            if (new_idx == (uint32_t)-1)
            {
                const auto sz = nodes.size();
                new_idx = (uint32_t)sz;
                cache.add_index(chunk_idx, tile_idx, new_idx);
                auto new_node = visited {
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

            if constexpr(Debug >= 3) [[unlikely]]
                DBG_nospace << " path:" << dist
                            << " pos:" << Vector3i(new_pt.coord())
                            << ";" << new_pt.offset();

            add_to_heap(new_idx, f_score, dist);
        }
    }

    //fm_debug_assert(nodes.size() == indexes.size());

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
        result.set_distance(Math::sqrt(closest_dist));
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
                           time, Math::sqrt(closest_dist), closest.dist, d0,
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
