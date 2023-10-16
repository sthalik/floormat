#include "path-search.hpp"
#include "object.hpp"
#include "point.hpp"
#include <Corrade/Containers/StaticArray.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

template<typename T> using bbox = path_search::bbox<T>;
using visited = astar::visited;

namespace {

constexpr auto div_size = path_search::div_size;

template<typename T>
requires std::is_arithmetic_v<T>
constexpr bbox<T> bbox_union(bbox<T> bb, Vector2i coord, Vector2b offset, Vector2ui size)
{
    auto center = coord * iTILE_SIZE2 + Vector2i(offset);
    auto min = center - Vector2i(size / 2);
    auto max = center + Vector2i(size);
    using Vec = VectorTypeFor<2, T>;
    return {
        .min = Math::min(Vec(bb.min), Vec(min)),
        .max = Math::max(Vec(bb.max), Vec(max)),
    };
}

template<typename T>
requires std::is_arithmetic_v<T>
constexpr auto bbox_from_pos(Math::Vector2<T> pos, Vector2b offset, Vector2ui size)
{
    const auto vec = Vector2i(pos) * iTILE_SIZE2 + Vector2i(offset);
    const auto min = vec - Vector2i(size / 2);
    const auto max = vec + Vector2i(size);
    const auto bb = bbox<float>{Vector2(min), Vector2(max)};
    return bb;
}

template<typename T>
requires std::is_arithmetic_v<T>
constexpr inline bbox<T> bbox_union(bbox<T> bb1, bbox<T> bb2)
{
    return { Math::min(bb1.min, bb2.min), Math::max(bb1.max, bb2.max) };
}

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
        vec *= div_size;
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
    dist += Math::abs(a.coord() - b.coord())*iTILE_SIZE2;
    dist += Vector2i(a.offset() - b.offset());
    return (uint32_t)Math::sqrt(dist.dot());
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
    std::push_heap(Q.begin(), Q.end(), heap_comparator{nodes});
}

uint32_t astar::pop_from_heap()
{
    std::pop_heap(Q.begin(), Q.end(), heap_comparator{nodes});
    const auto id = Q.back();
    Q.pop_back();
    return id;
}

path_search_result astar::Dijkstra(world& w, const point from_, const point to_, object_id own_id, uint32_t max_dist,
                                   Vector2ub own_size_, int debug, const pred& p)
{
#ifdef FM_NO_DEBUG
    (void)debug;
#endif

    clear();
    cache.allocate(from_, max_dist);

    constexpr auto min_size_ = Vector2ui{Vector2(div_size) * 1.5f + Vector2{.5f}};
    const auto own_size = Math::max(Vector2ui{Vector2{own_size_}*1.5f + Vector2{.5f}}, min_size_);
    const auto goal_distance = (uint32_t)(own_size * 2).length();

    const auto [from, from_offset] = from_;
    const auto [to, to_offset] = to_;

    if (from.z() != to.z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.z() != 0) [[unlikely]]
        return {};

    if (!path_search::is_passable(w, from, from_offset, own_size, own_id, p))
        return {};

    if (!path_search::is_passable(w, to, to_offset, own_size, own_id, p))
        return {};

    path_search_result result;
    auto& path = result.path(); path.clear();

    cache.allocate(from_, max_dist);
    cache.add_index(from_offset, from_, 0);
    nodes.push_back({.dist = 0, .pt = from_, });
    add_to_heap(0);

    if (!from_offset.isZero())
    {
        const auto from_local = Vector2i(from.local());
        const auto start_bbox = bbox_from_pos(Vector2(from_local), from_offset, own_size);
        const auto from_offset_len = Math::max(1u, (uint32_t)(Vector2(from_offset).length() + 0.5f));
        uint32_t idx = 1;
        constexpr int8_t div_min = (div_factor+1)/-2, div_max = div_factor/2;
        for (int8_t y = div_min; y < div_max; y++)
            for (int8_t x = div_min; x < div_max; x++)
            {
                const auto offset = Vector2b(div_size * Vector2i(x, y));
                if (offset != from_offset)
                    if (auto bb = bbox_union(start_bbox, from_local, offset, own_size);
                        path_search::is_passable(w, from.chunk3(), bb, own_id, p))
                    {
                        auto pt = point{from, offset};
                        cache.add_index(from_offset, pt, idx);
                        nodes.push_back({.dist = from_offset_len, .prev = 0, .pt = pt, });
                        add_to_heap(idx++);
                    }
            }
    }

    auto closest = distance(from_, to_);
    auto closest_pos = from_;
    uint32_t closest_path_len = 0;

    while (!Q.empty())
    {
        const auto id = pop_from_heap();
        point cur_pt;
        uint32_t cur_dist;
        {   auto& n = nodes[id];
            cur_pt = n.pt;
            cur_dist = n.dist;
        }

        if (auto d = distance(cur_pt, to_); d < closest)
        {
            closest = d;
            closest_pos = cur_pt;
            closest_path_len = cur_dist;
        }

#ifndef FM_NO_DEBUG
        if (debug >= 2) [[unlikely]]
            DBG_nospace << "node"
                        << " px:" << closest << " path:" << closest_path_len
                        << " pos:" << closest_pos;
#endif

        const auto bb0 = bbox_from_pos(Vector2(cur_pt.local()), cur_pt.offset(), own_size);
        for (auto [vec, len] : directions)
        {
            const auto dist = cur_dist + len;
            if (dist >= max_dist)
                continue;

            const auto new_pt = object::normalize_coords(cur_pt, vec);
            const auto [new_coord, new_offset] = new_pt;
            //const size_t new_pt_hash = new_pt.hash();

            bool fresh = true;

            auto chunk_idx = cache.get_chunk_index(Vector2i(new_pt.chunk()));
            auto tile_idx = cache.get_tile_index(from_offset, Vector2i(new_pt.local()), new_offset);
            auto new_idx = cache.lookup_index(chunk_idx, tile_idx);

            if (new_idx != (uint32_t)-1)
            {
                fresh = false;

                if (nodes[new_idx].dist <= dist)
                    continue;
            }

#if 1
            {   auto vec_ = Vector2(vec);
                auto bb1 = bbox<float>{ bb0.min + vec_, bb0.max + vec_ };
                auto bb = bbox_union(bb1, bb0);
                if (!path_search::is_passable(w, new_coord.chunk3(), bb, own_id, p))
                    continue;
            }
#else
            auto e = make_edge(cur_pt, new_pt);
            if (auto [it, fresh] = edges.try_emplace(e, edge_status::unknown); fresh)
            {
                auto& status = it.value();
                auto vec_ = Vector2(vec);
                auto bb1 = bbox<float>{ bb0.min + vec_, bb0.max + vec_ };
                auto bb = bbox_union(bb1, bb0);

                if (path_search::is_passable(w, new_coord.chunk3(), bb, own_id, p))
                    status = edge_status::good;
                else
                {
                    status = edge_status::bad;
                    continue;
                }
            }
#endif

            if (fresh)
            {   const auto sz = nodes.size();
                new_idx = (uint32_t)sz;
                cache.add_index(chunk_idx, tile_idx, new_idx);
                //indexes.insert({new_pt, new_idx}, new_pt_hash);
                auto new_node = visited {
                    .dist = dist, .prev = id,
                    .pt = {new_coord, new_offset},
                };
                nodes.push_back(new_node);
            }

#ifndef FM_NO_DEBUG
            if (debug >= 3) [[unlikely]]
                DBG_nospace << (fresh ? "" : " old")
                            << " path:" << dist
                            << " pos:" << Vector3i(new_coord)
                            << ";" << new_offset;
#endif

            add_to_heap(new_idx);
        }
    }

    //fm_debug_assert(nodes.size() == indexes.size());
#ifndef FM_NO_DEBUG
    if (debug >= 1)
        DBG_nospace << "dijkstra: closest px:" << closest
                    << " path:" << closest_path_len
                    << " pos:" << closest_pos
                    << " nodes:" << nodes.size();
#endif

    // todo...
    return result;
}

struct astar::chunk_cache
{
    static constexpr size_t dimensions[] = {
        2 /* offset, no-offset*/,
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

    struct index { uint32_t value = (uint32_t)value; };

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
    {
        for (auto i = 0uz; i < len; i++)
        {
            array[i].exists = {};
            array[i].indexes = {}; // todo
        }
    }
}

size_t astar::cache::get_chunk_index(Vector2i start, Vector2ui size, Vector2i coord)
{
    auto off_ =  coord - start;
    fm_assert(off_ >= Vector2i{0, 0});
    auto off = Vector2ui(off_);
    fm_assert(off < size);
    auto index = off.y() * size.x() + off.x();
    fm_debug_assert(index < size.product());
    return index;
}

size_t astar::cache::get_chunk_index(Vector2i chunk) const { return get_chunk_index(start, size, chunk); }

size_t astar::cache::get_tile_index(Vector2b from_offset, Vector2i pos, Vector2b offset_)
{
    Vector2i offset{offset_};
    bool is_offset = false;
    if (offset % div_size != Vector2i{0, 0})
    {
        offset -= Vector2i(from_offset);
        fm_assert(offset % div_size == Vector2i{0, 0});
        is_offset = true;
    }
    constexpr auto tile_start = div_size * div_factor/-2;
    offset -= tile_start;
    fm_debug_assert(offset >= Vector2i{0, 0});
    auto nth_div = offset / div_size;
    const size_t idx[] = {
        (size_t)is_offset,
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
    fm_assert(index < chunk_cache::size);
    return index;
}

void astar::cache::add_index(size_t chunk_index, size_t tile_index, uint32_t index)
{
    auto& c = array[chunk_index];
    fm_debug_assert(!c.exists[tile_index]);
    c.exists[tile_index] = true;
    c.indexes[tile_index] = {index};
}

void astar::cache::add_index(Vector2b from_offset, point pt, uint32_t index)
{
    auto ch = get_chunk_index(Vector2i(pt.chunk()));
    auto tile = get_tile_index(from_offset, Vector2i(pt.local()), pt.offset());
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
