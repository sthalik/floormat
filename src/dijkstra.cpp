#include "path-search.hpp"
#include "compat/format.hpp"
#include "compat/debug.hpp"
#include "compat/heap.hpp"
#include "object.hpp"
#include "point.hpp"
#include <cstdio>
#include <Corrade/Containers/StaticArray.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Timeline.h>

namespace floormat {

template<typename T> using bbox = path_search::bbox<T>;
using visited = astar::visited;

namespace {

constexpr auto div_size = path_search::div_size;

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
    const std::vector<visited>& nodes; // NOLINT
    inline heap_comparator(const std::vector<visited>& nodes) : nodes{nodes} {}
    inline bool operator()(uint32_t a, uint32_t b) const { return nodes[b].dist < nodes[a].dist; }
};

inline uint32_t distance(point a, point b)
{
    Vector2i dist;
    dist += (a.coord() - b.coord())*iTILE_SIZE2;
    dist += Vector2i(a.offset()) - Vector2i(b.offset());
    return (uint32_t)Math::ceil(Math::sqrt(Vector2(dist).dot()));
}

inline uint32_t distance_l2(point a, point b)
{
    Vector2i dist;
    dist += (a.coord() - b.coord())*iTILE_SIZE2;
    dist += Vector2i(a.offset()) - Vector2i(b.offset());
    return (uint32_t)Math::abs(dist).sum();
}

void set_result_from_idx(path_search_result& result, const std::vector<visited>& nodes,
                         point to, const uint32_t idx)
{
    fm_debug_assert(idx != (uint32_t)-1);

    auto& path = result.path();
    path.clear();

    const auto& to_node = nodes[idx];
    if (to != to_node.pt)
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

void astar::reserve(size_t capacity)
{
    nodes.reserve(capacity);
    Q.reserve(capacity);
}

void astar::clear()
{
    nodes.clear();
    Q.clear();
}

void astar::add_to_heap(uint32_t id)
{
    Q.push_back(id);
    Heap::push_heap(Q.begin(), Q.end(), heap_comparator{nodes});
}

uint32_t astar::pop_from_heap()
{
    Heap::pop_heap(Q.begin(), Q.end(), heap_comparator{nodes});
    const auto id = Q.back();
    Q.pop_back();
    return id;
}

path_search_result astar::Dijkstra(world& w, const point from, const point to,
                                   object_id own_id, uint32_t max_dist, Vector2ub own_size_,
                                   int debug, const pred& p)
{
#ifdef FM_NO_DEBUG
    (void)debug;
#endif

    Timeline timeline; timeline.start();

    clear();
    cache.allocate(from, max_dist);

    constexpr auto min_size = Vector2ui{div_size};
    const auto own_size = Math::max(Vector2ui(own_size_), min_size);
    constexpr auto goal_thres = (uint32_t)(div_size.length() + 1.5f);

    if (from.coord().z() != to.coord().z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.coord().z() != 0) [[unlikely]]
        return {};

    if (!path_search::is_passable(w, from.coord(), from.offset(), own_size, own_id, p))
        return {};

    if (!path_search::is_passable(w, to.coord(), to.offset(), own_size, own_id, p))
        return {};

    constexpr int8_t div_min = -div_factor*2, div_max = div_factor*2;

    for (int8_t y = div_min; y <= div_max; y++)
        for (int8_t x = div_min; x <= div_max; x++)
        {
            constexpr auto min_dist = (uint32_t)((TILE_SIZE2*2.f).length() + 1.f);
            auto off = Vector2i(x, y) * div_size;
            auto pt = object::normalize_coords({from.coord(), {}}, off);
            auto bb = bbox<float>(bbox_from_pos2(from, pt, own_size));
            auto dist = distance(from, pt) + min_dist;

            if (path_search::is_passable(w, from.chunk3(), bb, own_id, p))
            {
                auto idx = (uint32_t)nodes.size();
                cache.add_index(pt, idx);
                nodes.push_back({.dist = dist, .prev = (uint32_t)-1, .pt = pt, });
                add_to_heap(idx);
            }
        }

    auto closest_dist = (uint32_t)-1;
    uint32_t closest_idx = (uint32_t)-1;
    auto goal_idx = (uint32_t)-1;

    while (!Q.empty())
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

        if (auto d = distance(cur_pt, to); d < closest_dist) [[unlikely]]
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

        if (auto dist_to_goal = distance_l2(cur_pt, to); dist_to_goal < goal_thres) [[unlikely]]
        {
            auto dist = cur_dist + dist_to_goal;
            if (auto bb = bbox<float>(bbox_from_pos2(to, cur_pt, own_size));
                path_search::is_passable(w, to.chunk3(), bb, own_id, p))
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
                !path_search::is_passable(w, new_pt.chunk3(), bb, own_id, p))
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
                nodes.push_back(new_node);
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
        const auto time = (uint32_t)Math::ceil(result.time() * 1e3f);
        if (goal_idx != (uint32_t)-1)
        {
            auto d = nodes[goal_idx].dist;
            len = snformat(buf, "Dijkstra: found in {} ms "
                                "len:{} len0:{} ratio:{:.4}\n"_cf,
                           time, d, d0,
                           d > 0 && d0 > 0 ? (float)d/(float)d0 : 1);
        }
        else if (closest_idx != (uint32_t)-1)
        {
            const auto& closest = nodes[closest_idx];
            fm_assert(closest.dist != 0 && closest.dist != (uint32_t)-1);
            len = snformat(buf, "Dijkstra: no path found in {} ms "
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

struct astar::chunk_cache
{
    static constexpr size_t dimensions[] = {
        TILE_COUNT,
        (size_t)div_factor * (size_t)div_factor,
    };
    static constexpr size_t size = []() constexpr -> size_t {
        size_t x = 1;
        for (auto i : dimensions)
            x *= i;
        return x;
    }();
    static constexpr size_t rank = sizeof(dimensions)/sizeof(dimensions[0]);

    struct index { uint32_t value = 0; };
    std::array<index, size> indexes = {};
    std::bitset<size> exists{false};
};

astar::cache::cache() = default;

Vector2ui astar::cache::get_size_to_allocate(uint32_t max_dist)
{
    constexpr auto chunk_size = Vector2ui(iTILE_SIZE2) * TILE_MAX_DIM;
    constexpr auto rounding   = chunk_size - Vector2ui(1);
    auto nchunks = (Vector2ui(max_dist) + rounding) / chunk_size;
    return nchunks + Vector2ui(3);
}

void astar::cache::allocate(point from, uint32_t max_dist)
{
    auto off = get_size_to_allocate(max_dist);
    start = Vector2i(from.chunk()) - Vector2i(off);
    size = off * 2u + Vector2ui(1);
    auto len = size.product();
    if (len > array.size())
        array = Array<chunk_cache>{ValueInit, len};
    else
        for (auto i = 0uz; i < len; i++)
            array[i].exists = {};
}

size_t astar::cache::get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord)
{
    auto off = Vector2ui(coord - start);
    fm_assert(off < size);
    auto index = off.y() * size.x() + off.x();
    fm_debug_assert(index < size.product());
    return index;
}

size_t astar::cache::get_chunk_index(Vector2i chunk) const { return get_chunk_index(start, size, chunk); }

size_t astar::cache::get_tile_index(Vector2i pos, Vector2b offset_)
{
    Vector2i offset{offset_};
    constexpr auto tile_start = div_size * div_factor/-2;
    offset -= tile_start;
    fm_debug_assert(offset >= Vector2i{0, 0} && offset < div_size * div_factor);
    auto nth_div = Vector2ui(offset) / Vector2ui(div_size);
    const size_t idx[] = {
        (size_t)pos.y() * TILE_MAX_DIM + (size_t)pos.x(),
        (size_t)nth_div.y() * div_factor + (size_t)nth_div.x(),
    };
    size_t index = 0;
    for (auto i = 0uz; i < chunk_cache::rank; i++)
    {
        size_t k = idx[i];
        for (auto j = 0uz; j < i; j++)
            k *= chunk_cache::dimensions[j];
        index += k;
    }
    fm_debug_assert(index < chunk_cache::size);
    return index;
}

void astar::cache::add_index(size_t chunk_index, size_t tile_index, uint32_t index)
{
    fm_debug_assert(index != (uint32_t)-1);
    auto& c = array[chunk_index];
    fm_debug_assert(!c.exists[tile_index]);
    c.exists[tile_index] = true;
    c.indexes[tile_index] = {index};
}

void astar::cache::add_index(point pt, uint32_t index)
{
    auto ch = get_chunk_index(Vector2i(pt.chunk()));
    auto tile = get_tile_index(Vector2i(pt.local()), pt.offset());
    fm_debug_assert(!array[ch].exists[tile]);
    array[ch].exists[tile] = true;
    array[ch].indexes[tile] = {index};
}

uint32_t astar::cache::lookup_index(size_t chunk_index, size_t tile_index)
{
    auto& c = array[chunk_index];
    if (c.exists[tile_index])
        return c.indexes[tile_index].value;
    else
        return (uint32_t)-1;
}

} // namespace floormat
