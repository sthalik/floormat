#include "path-search.hpp"
#include "object.hpp"
#include "point.hpp"
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

template<typename T> using bbox = path_search::bbox<T>;

namespace {

constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
constexpr auto div_size = path_search::div_size;
constexpr auto min_size = path_search::min_size;
constexpr auto goal_distance = div_size;

template<typename T>
requires std::is_arithmetic_v<T>
constexpr bbox<T> bbox_union(bbox<T> bb, Vector2i coord, Vector2b offset, Vector2ub size)
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
constexpr bbox<T> bbox_from_pos(Math::Vector<2, T> pos, Vector2b offset, Vector2ub size)
{
    using Vec = VectorTypeFor<2, T>;
    constexpr auto tile_size = Vec(iTILE_SIZE2);
    const auto vec = pos * tile_size + Vec(offset);
    const auto bb = bbox<float>{vec - Vec(size >> 1), vec + Vec(size)};
    return bb;
}

class heap_comparator
{
    const std::vector<astar::visited>& nodes; // NOLINT

public:
    heap_comparator(const std::vector<astar::visited>& nodes) : nodes{nodes} {}

    inline bool operator()(uint32_t a, uint32_t b) const
    {
        fm_debug_assert(std::max(a, b) < nodes.size());
        const auto& n1 = nodes[a];
        const auto& n2 = nodes[b];
        return n2.dist < n1.dist;
    }
};

class box
{
    using visited = astar::visited;
    std::vector<visited>& vec; // NOLINT
    uint32_t id;

public:
    fm_DECLARE_DELETED_COPY_ASSIGNMENT(box);
    fm_DECLARE_DELETED_MOVE_ASSIGNMENT(box);

    visited& operator*() { return vec[id]; }
    visited* operator->() { return &vec[id]; }

    box(std::vector<visited>& vec, uint32_t id) : vec{vec}, id{id} {}
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
    edges.reserve(capacity*4);
    Q.reserve(capacity);
}

void astar::clear()
{
    nodes.clear();
    indexes.clear();
    edges.clear();
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

auto astar::make_edge(const point& a, const point& b) -> edge
{
    if (a < b)
        return { a.coord, b.coord, a.offset, b.offset };
    else
        return { b.coord, a.coord, b.offset, a.offset };
}

bool astar::edge::operator==(const floormat::astar::edge& other) const = default;

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

path_search_result astar::Dijkstra(world& w, Vector2ub own_size, const object_id own_id,
                                   point from_, point to_, uint32_t max_dist, const pred& p)
{
    const auto [from, from_offset] = from_;
    const auto [to, to_offset] = to_;

    own_size = Math::max(own_size, Vector2ub(min_size));

    if (from.z() != to.z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.z() != 0) [[unlikely]]
        return {};

    if (!path_search::is_passable(w, from, from_offset, own_size, own_id, p))
        return {};

    if (!path_search::is_passable(w, to, to_offset, own_size, own_id, p))
        return {};

    clear();

    const auto start_bbox = bbox_from_pos(Vector2(from.local()), from_offset, own_size);

    path_search_result result;
    auto& path = result._node->vec; path.clear();

    indexes[from_] = 0;
    nodes.push_back({.dist = 0, .coord = from, .offset = from_offset });
    add_to_heap(0);

    if (!from_offset.isZero())
    {
        const auto from_offset_len = Math::max(1u, (uint32_t)(Vector2(from_offset).length() + 0.5f));
        uint32_t idx = 1;
        // todo also add 4 subdivisions within the tile the same way
        if (auto bb = bbox_union(start_bbox, Vector2i(from.local()), {}, own_size);
            path_search::is_passable(w, chunk_coords_{from}, bb, own_id, p))
        {
            indexes[{from, {}}] = idx;
            nodes.push_back({.dist = from_offset_len, .prev = 0, .coord = from, .offset = {}});
            add_to_heap(idx++);
        }
    }

    auto closest = (uint32_t)-1;

    while (!Q.empty())
    {
        const auto id = pop_from_heap();
        auto node = box{nodes, id};

        if (auto d = distance({node->coord, node->offset}, {to, to_offset}); d < closest)
            closest = d;

        //Debug{} << "node" << id << "|" << node.coord.to_signed3() << node.offset << "|" << node.dist;

        const auto bb0 = bbox_from_pos(Vector2(node->coord.local()), node->offset, own_size);

        for (auto [vec, len] : directions)
        {
            auto [new_coord, new_offset] = object::normalize_coords(node->coord, node->offset, vec);
            const auto dist = node->dist + len;
            if (dist >= max_dist)
                continue;

            const auto sz = nodes.size();
            auto [it, fresh] = indexes.try_emplace({.coord = new_coord, .offset = new_offset}, sz);
            const auto new_idx = it.value();

            if (new_idx == sz)
            {
                auto new_node = astar::visited {
                    .dist = dist, .prev = id,
                    .coord = new_coord, .offset = new_offset,
                };
                nodes.push_back(new_node);
            }

            auto& node = nodes[new_idx];

            if (!fresh && dist >= node.dist)
                continue;
            node.dist = dist;

            auto e = make_edge({node.coord, node.offset}, {new_coord, new_offset});
            if (auto [it, fresh] = edges.try_emplace(e, edge_status::unknown); fresh)
            {
                auto& status = it.value();
                auto vec_ = Vector2(vec);
                auto bb1 = bbox<float>{ bb0.min + vec_, bb0.max + vec_ };
                auto bb = bbox_union(bb1, bb0);

                if (path_search::is_passable(w, chunk_coords_(node.coord), bb, own_id, p))
                    status = edge_status::good;
                else
                {
                    status = edge_status::bad;
                    continue;
                }
            }

            //Debug{} << (fresh ? "   new" : "  old") << new_idx << "|" << node.coord.to_signed3() << node.offset << "|" << node.dist;

            add_to_heap(new_idx);
        }
    }

    //DBG << "done!" << nodes.size() << "nodes," << indexes.size() << "indexes," << edges.size() << "edges.";

    Debug{} << "closest" << closest;
    // todo...
    return result;
}

} // namespace floormat
