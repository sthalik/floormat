#include "path-search.hpp"
#include "object.hpp"
#include "point.hpp"
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
constexpr bbox<T> bbox_union(bbox<T> bb1, bbox<T> bb2)
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

template<typename T>
requires std::is_arithmetic_v<T>
constexpr auto bbox_from_pos(Math::Vector<2, T> pos, Vector2b offset, Vector2ui size)
{
    const auto vec = Vector2i(pos) * iTILE_SIZE2 + Vector2i(offset);
    const auto min = vec - Vector2i(size / 2);
    const auto max = vec + Vector2i(size);
    const auto bb = bbox<float>{Vector2(min), Vector2(max)};
    return bb;
}

struct heap_comparator
{
    const std::vector<visited>& nodes; // NOLINT
    inline heap_comparator(const std::vector<visited>& nodes) : nodes{nodes} {}
    inline bool operator()(uint32_t a, uint32_t b) const { return nodes[b].dist < nodes[a].dist; }
};

uint32_t distance(point a, point b)
{
    Vector2i dist;
    dist += Math::abs(a.coord - b.coord)*iTILE_SIZE2;
    dist += Vector2i(a.offset - b.offset);
    return (uint32_t)Math::sqrt(dist.dot());
}

} // namespace

astar::astar()
{
    indexes.max_load_factor(.4f);
    reserve(initial_capacity);
}

void astar::reserve(size_t capacity)
{
    nodes.reserve(capacity);
    indexes.reserve(capacity);
#if !FM_ASTAR_NO_EDGE_CACHE
    edges.reserve(capacity*4);
#endif
    Q.reserve(capacity);
}

void astar::clear()
{
    nodes.clear();
    indexes.clear();
#if !FM_ASTAR_NO_EDGE_CACHE
    edges.clear();
#endif
    Q.clear();
}

void astar::add_to_heap(uint32_t id)
{
    Q.push_back(id);
    std::push_heap(Q.begin(), Q.end(), heap_comparator(nodes));
}

uint32_t astar::pop_from_heap()
{
    std::pop_heap(Q.begin(), Q.end(), heap_comparator(nodes));
    const auto id = Q.back();
    Q.pop_back();
    return id;
}

#ifndef FM_ASTAR_NO_EDGE_CACHE
auto astar::make_edge(const point& a, const point& b) -> edge
{
    if (a < b)
        return { a.coord, b.coord, a.offset, b.offset };
    else
        return { b.coord, a.coord, b.offset, a.offset };
}

size_t astar::edge_hash::operator()(const edge& e) const
{
    static_assert(sizeof e == 8 + 8 + 2 + 2);
#ifdef FLOORMAT_64
    static_assert(sizeof nullptr > 4);
    return fnvhash_64(&e, sizeof e);
#else
    static_assert(sizeof nullptr == 4);
    return fnvhash_32(&e, sizeof e);
#endif
}

bool astar::edge::operator==(const floormat::astar::edge& other) const = default;
#endif

size_t astar::point_hash::operator()(point pt) const
{
    static_assert(sizeof(global_coords) == 8);
#ifdef FLOORMAT_64
    static_assert(sizeof nullptr > 4);
    return fnvhash_64(&pt, sizeof pt);
#else
    static_assert(sizeof nullptr == 4);
    return fnvhash_32(&pt, sizeof pt);
#endif
}

path_search_result astar::Dijkstra(world& w, point from_, point to_, object_id own_id, uint32_t max_dist,
                                   Vector2ub own_size_, int debug, const pred& p)
{
#ifdef FM_NO_DEBUG
    (void)debug;
#endif

    clear();

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

    const auto from_local = Vector2i(from.local());

    if (!path_search::is_passable(w, from, from_offset, own_size, own_id, p))
        return {};

    if (!path_search::is_passable(w, to, to_offset, own_size, own_id, p))
        return {};

    const auto start_bbox = bbox_from_pos(Vector2(from_local), from_offset, own_size);

    path_search_result result;
    auto& path = result._node->vec; path.clear();

    indexes[from_] = 0;
    nodes.push_back({.dist = 0, .coord = from, .offset = from_offset });
    add_to_heap(0);

    if (!from_offset.isZero())
    {
        const auto from_offset_len = Math::max(1u, (uint32_t)(Vector2(from_offset).length() + 0.5f));
        uint32_t idx = 1;
        constexpr int8_t div_min = (div_factor+1)/-2, div_max = div_factor/2;
        for (int8_t y = div_min; y < div_max; y++)
            for (int8_t x = div_min; x < div_max; x++)
            {
                const auto offset = Vector2b(div_size * Vector2i(x, y));
                if (auto bb = bbox_union(start_bbox, from_local, offset, own_size);
                    path_search::is_passable(w, chunk_coords_{from}, bb, own_id, p))
                {
                    indexes[{from, offset}] = idx;
                    nodes.push_back({.dist = from_offset_len, .prev = 0, .coord = from, .offset = offset});
                    add_to_heap(idx++);
                }
            }
    }

    auto closest = distance({from, from_offset}, {to, to_offset});
    auto closest_pos = point{from, from_offset};
    uint32_t closest_path_len = 0;

    while (!Q.empty())
    {
        const auto id = pop_from_heap();
        point n_pt;
        global_coords n_coord;
        Vector2b n_offset;
        uint32_t n_dist;
        {   auto& n = nodes[id];
            n_coord = n.coord;
            n_offset = n.offset;
            n_pt = {n_coord, n_offset};
            n_dist = n.dist;
        }

        if (auto d = distance({n_coord, n_offset}, {to, to_offset}); d < closest)
        {
            closest = d;
            closest_pos = {n_coord, n_offset};
            closest_path_len = n_dist;
        }

#ifndef FM_NO_DEBUG
        if (debug >= 2) [[unlikely]]
            DBG_nospace << "node"
                        << " px:" << closest << " path:" << closest_path_len
                        << " pos:" << closest_pos.coord.to_signed()
                        << ";" << closest_pos.offset;
#endif

        const auto bb0 = bbox_from_pos(Vector2(n_coord.local()), n_offset, own_size);
        for (auto [vec, len] : directions)
        {
            auto [new_coord, new_offset] = object::normalize_coords(n_coord, n_offset, vec);
            const auto dist = n_dist + len;
            if (dist >= max_dist)
                continue;

            const auto new_pt = point{.coord = new_coord, .offset = new_offset};

            auto new_idx = (uint32_t)-1;
            bool fresh = true;

            if (auto it = indexes.find(new_pt); it != indexes.end())
            {
                new_idx = it->second;
                fresh = false;

                if (nodes[new_idx].dist <= dist)
                    continue;
            }

#ifdef FM_ASTAR_NO_EDGE_CACHE
            {   auto vec_ = Vector2(vec);
                auto bb1 = bbox<float>{ bb0.min + vec_, bb0.max + vec_ };
                auto bb = bbox_union(bb1, bb0);
                if (!path_search::is_passable(w, chunk_coords_(new_coord), bb, own_id, p))
                    continue;
            }
#else
            auto e = make_edge(n_pt, {new_coord, new_offset});
            if (auto [it, fresh] = edges.try_emplace(e, edge_status::unknown); fresh)
            {
                auto& status = it.value();
                auto vec_ = Vector2(vec);
                auto bb1 = bbox<float>{ bb0.min + vec_, bb0.max + vec_ };
                auto bb = bbox_union(bb1, bb0);

                if (path_search::is_passable(w, chunk_coords_(new_coord), bb, own_id, p))
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
                new_idx = indexes[new_pt] = (uint32_t)sz;
                auto new_node = visited {
                    .dist = dist, .prev = id,
                    .coord = new_coord, .offset = new_offset,
                };
                nodes.push_back(new_node);
            }

#ifndef FM_NO_DEBUG
            if (debug >= 3) [[unlikely]]
                DBG_nospace << (fresh ? "" : " old")
                            << " path:" << closest_path_len
                            << " pos:" << closest_pos.coord.to_signed()
                            << ";" << closest_pos.offset;
#endif

            add_to_heap(new_idx);
        }
    }

    fm_debug_assert(nodes.size() == indexes.size());
#ifndef FM_NO_DEBUG
    if (debug >= 1)
        DBG_nospace << "dijkstra: closest px:" << closest << " path:" << closest_path_len
                    << " pos:" << closest_pos.coord.to_signed() << ";" << closest_pos.offset
                    << " nodes:" << nodes.size()
#ifndef FM_ASTAR_NO_EDGE_CACHE
                    << " edges:" << edges.size()
#endif
            ;
#endif

    // todo...
    return result;
}

} // namespace floormat