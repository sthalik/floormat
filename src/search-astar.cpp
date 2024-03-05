#include "search-astar.hpp"
#include "search-bbox.hpp"
#include "search-constants.hpp"
#include "search-cache.hpp"
#include "search-result.hpp"
#include "compat/format.hpp"
#include "compat/vector-wrapper.hpp"
#include "compat/heap.hpp"
#include "object.hpp"
#include "world.hpp"
#include "point.inl"
#include <cstdio>
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/StaticArray.h> // todo remove
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Timeline.h>

namespace floormat {

struct astar::visited
{
    uint32_t dist = (uint32_t)-1;
    uint32_t prev = (uint32_t)-1;
    point pt;
};

using visited = astar::visited;
using Search::bbox;
using Search::div_size;
using Search::div_factor;
using Search::min_size;

namespace {

constexpr bbox<Int> bbox_from_pos1(point pt, Vector2ui size)
{
    auto center = Vector2i(pt.local()) * iTILE_SIZE2 + Vector2i(pt.offset());
    auto top_left = center - Vector2i(size / 2);
    auto bottom_right = top_left + Vector2i(size);
    return { top_left, bottom_right };
}

constexpr bbox<Int> bbox_from_pos2(point pt, point from, Vector2ui size)
{
    constexpr auto chunk_size = iTILE_SIZE2 * (int)TILE_MAX_DIM;
    auto nchunks = from.chunk() - pt.chunk();
    auto chunk_pixels = nchunks * chunk_size;

    auto bb0_ = bbox_from_pos1(from, size);
    auto bb0 = bbox<Int>{ bb0_.min + chunk_pixels, bb0_.max + chunk_pixels };
    auto bb = bbox_from_pos1(pt, size);

    auto min = Math::min(bb0.min, bb.min);
    auto max = Math::max(bb0.max, bb.max);

    return { min, max };
}

static_assert(bbox_from_pos1({{}, {0, 0}, {15, 35}}, {10, 20}) == bbox<Int>{{10, 25}, {20, 45}});
static_assert(bbox_from_pos2({{{1, 1}, {1, 15}, 0}, {1, -1}},
                             {{{1, 2}, {1,  0}, 0}, {1, -1}},
                             {256, 256}) == bbox<Int>{{-63, 831}, {193, 1151}});

constexpr auto directions = []() constexpr
{
    struct pair { Vector2i dir; uint32_t len; };
    constexpr auto len1 = div_size;
    constexpr auto len2 = (uint32_t)(len1.length() + 0.5f); // NOLINT
    std::array<pair, 8> array = {{
        { { -1, -1 }, len2 },
        { {  1,  1 }, len2 },
        { { -1,  1 }, len2 },
        { {  1, -1 }, len2 },
        { { -1,  0 }, len1.x() },
        { {  0, -1 }, len1.y() },
        { {  1,  0 }, len1.x() },
        { {  0,  1 }, len1.y() },
    }};
    for (auto& [vec, len] : array)
    {
        vec *= div_size;
        vec += Vector2i(1);
    }
#if 0
    for (auto i = 0uz; i < array.size(); i++)
        for (auto j = 0uz; j < i; j++)
            fm_assert(array[i].dir != array[j].dir);
#endif
    return array;
}();

struct heap_comparator
{
    const Array<visited>& nodes; // NOLINT
    inline heap_comparator(const Array<visited>& nodes) : nodes{nodes} {}
    inline bool operator()(uint32_t a, uint32_t b) const { return nodes[b].dist < nodes[a].dist; }
};

void set_result_from_idx(path_search_result& result, const Array<visited>& nodes,
                         point to, const uint32_t idx)
{
    fm_debug_assert(idx != (uint32_t)-1);

    auto& path = result.raw_path().vec;
    path.clear();

    const auto& to_node = nodes[idx];
    if (result.is_found() && to != to_node.pt)
        path.push_back(to);
    result.set_cost(to_node.dist);

    auto i = idx;
    do {
        const auto& node = nodes[i];
        path.push_back(node.pt);
        i = node.prev;
    } while (i != (uint32_t)-1);

    std::reverse(path.begin(), path.end());
}

} // namespace

astar::astar()
{
    reserve(initial_capacity);
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

void astar::add_to_heap(uint32_t id)
{
    arrayAppend(Q, id);
    Heap::push_heap(Q.begin(), Q.end(), heap_comparator{nodes});
}

uint32_t astar::pop_from_heap()
{
    Heap::pop_heap(Q.begin(), Q.end(), heap_comparator{nodes});
    const auto id = Q.back();
    arrayRemoveSuffix(Q);
    return id;
}

path_search_result astar::Dijkstra(world& w, const point from, const point to,
                                   object_id own_id, uint32_t max_dist, Vector2ub own_size_,
                                   int debug, const pred& p)
{
#ifdef FM_NO_DEBUG
    (void)debug;
#endif

    Timeline timeline;
    if (debug > 0) [[unlikely]]
        timeline.start();

    clear();
    auto& cache = *_cache;
    cache.allocate(from, max_dist);

    const auto own_size = Math::max(Vector2ui(own_size_), min_size);
    constexpr auto goal_thres = (uint32_t)(div_size.length() + 1.5f);

    if (from.coord().z() != to.coord().z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.coord().z() != 0) [[unlikely]]
        return {};

    if (!path_search::is_passable(w, cache, from.coord(), from.offset(), own_size, own_id, p))
        return {};

    if (!path_search::is_passable(w, cache, to.coord(), to.offset(), own_size, own_id, p))
        return {};

    constexpr int8_t div_min = -div_factor*2, div_max = div_factor*2;

    for (int8_t y = div_min; y <= div_max; y++)
        for (int8_t x = div_min; x <= div_max; x++)
        {
            constexpr auto min_dist = (uint32_t)((TILE_SIZE2*2.f).length() + 1.f);
            auto off = Vector2i(x, y) * div_size;
            auto pt = object::normalize_coords({from.coord(), {}}, off);
            auto bb = bbox<float>(bbox_from_pos2(from, pt, own_size));
            auto dist = point::distance(from, pt) + min_dist;

            if (path_search::is_passable(w, cache, from.chunk3(), bb, own_id, p))
            {
                auto idx = (uint32_t)nodes.size();
                cache.add_index(pt, idx);
                arrayAppend(nodes, {.dist = dist, .prev = (uint32_t)-1, .pt = pt, });
                add_to_heap(idx);
            }
        }

    auto closest_dist = (uint32_t)-1;
    uint32_t closest_idx = (uint32_t)-1;
    auto goal_idx = (uint32_t)-1;

    while (!Q.isEmpty())
    {
        const auto cur_idx = pop_from_heap();
        point cur_pt;
        uint32_t cur_dist;
        {   auto& n = nodes[cur_idx];
            cur_pt = n.pt;
            cur_dist = n.dist;
        }

        if (cur_dist >= max_dist) [[unlikely]]
            continue;

        if (auto d = point::distance(cur_pt, to); d < closest_dist) [[unlikely]]
        {
            closest_dist = d;
            closest_idx = cur_idx;

#ifndef FM_NO_DEBUG
            if (debug >= 2) [[unlikely]]
                DBG_nospace << "closest node"
                            << " px:" << closest_dist << " path:" << cur_dist
                            << " pos:" << cur_pt;
#endif
        }

        if (auto dist_to_goal = point::distance_l2(cur_pt, to); dist_to_goal < goal_thres) [[unlikely]]
        {
            auto dist = cur_dist + dist_to_goal;
            if (auto bb = bbox<float>(bbox_from_pos2(to, cur_pt, own_size));
                path_search::is_passable(w, cache, to.chunk3(), bb, own_id, p))
            {
                goal_idx = cur_idx;
                max_dist = dist;
                continue; // path can only get longer
            }
        }

        for (auto [vec, len] : directions)
        {
            const auto dist = cur_dist + len;
            if (dist >= max_dist)
                continue;

            const auto new_pt = object::normalize_coords(cur_pt, vec);
            auto chunk_idx = cache.get_chunk_index(Vector2i(new_pt.chunk()));
            auto tile_idx = cache.get_tile_index(Vector2i(new_pt.local()), new_pt.offset());
            auto new_idx = cache.lookup_index(chunk_idx, tile_idx);

            if (new_idx != (uint32_t)-1)
            {
                if (nodes[new_idx].dist <= dist)
                    continue;
            }

            if (auto bb = bbox<float>(bbox_from_pos2(new_pt, cur_pt, own_size));
                !path_search::is_passable(w, cache, new_pt.chunk3(), bb, own_id, p))
                continue;

            if (new_idx == (uint32_t)-1)
            {
                const auto sz = nodes.size();
                new_idx = (uint32_t)sz;
                cache.add_index(chunk_idx, tile_idx, new_idx);
                auto new_node = visited {
                    .dist = dist, .prev = cur_idx,
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

#ifndef FM_NO_DEBUG
            if (debug >= 3) [[unlikely]]
                DBG_nospace << " path:" << dist
                            << " pos:" << Vector3i(new_pt.coord())
                            << ";" << new_pt.offset();
#endif

            add_to_heap(new_idx);
        }
    }

    //fm_debug_assert(nodes.size() == indexes.size());

    path_search_result result;

    if (goal_idx != (uint32_t)-1)
    {
        result.set_found(true);
        result.set_distance(0);
        set_result_from_idx(result, nodes, to, goal_idx);
    }
    else if (closest_idx != (uint32_t)-1)
    {
        result.set_found(false);
        result.set_distance(closest_dist);
        set_result_from_idx(result, nodes, to, closest_idx);
    }

    result.set_time(timeline.currentFrameTime());

#ifndef FM_NO_DEBUG
    if (debug >= 1) [[unlikely]]
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
            len = snformat(buf, "Dijkstra: found in {:.1f} ms "
                                "len:{} len0:{} ratio:{:.4}\n"_cf,
                           time, d, d0,
                           d > 0 && d0 > 0 ? (float)d/(float)d0 : 1);
        }
        else if (closest_idx != (uint32_t)-1)
        {
            const auto& closest = nodes[closest_idx];
            fm_assert(closest.dist != 0 && closest.dist != (uint32_t)-1);
            len = snformat(buf, "Dijkstra: no path found in {:.1f} ms "
                                "closest:{} len:{} len0:{} ratio:{:.4}\n"_cf,
                           time, closest_dist, closest.dist, d0,
                           d0 > 0 ? (float)closest.dist/(float)d0 : 1);
        }
        if (len)
        {
            len = Math::min(len, std::size(buf)-1);
            std::fwrite(buf, len, 1, stdout);
            std::fflush(stdout);
        }
    }
#endif

    return result;
}

} // namespace floormat
