#include "path-search.hpp"
#include "object.hpp"
#include "compat/math.hpp"
#include <Corrade/Containers/PairStl.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

template<typename T> using bbox = path_search::bbox<T>;

namespace {

constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
constexpr auto div_size = path_search::div_size;
constexpr auto min_size = path_search::min_size;

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

constexpr auto directions = [] constexpr
{
    struct pair { Vector2i dir; uint32_t len; };
    constexpr auto len1 = div_size;
    constexpr auto len2 = (uint32_t)(math::sqrt((float)len1.dot()) + 0.5f); // NOLINT
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

} // namespace

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
    auto heap_comparator = [this](uint32_t a, uint32_t b) {
        fm_debug_assert(std::max(a, b) < nodes.size());
        const auto& n1 = nodes[a];
        const auto& n2 = nodes[b];
        return n2.dist < n1.dist;
    };

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
    fm_debug_assert(nodes.empty());

    const auto start_bbox = bbox_from_pos(Vector2(from.local()), from_offset, own_size);
    const auto from_offset_len = Math::max(1u, (uint32_t)(Vector2(from_offset).length() + 0.5f));

    path_search_result result;
    fm_debug_assert(result._node); // todo
    auto& path = result._node->vec; path.clear();

    indexes[from_] = 0;
    nodes.push_back({.dist = 0, .coord = from, .offset = from_offset });
    Q.push_back(0);
    std::push_heap(Q.begin(), Q.end(), heap_comparator);

    if (!from_offset.isZero())
    {
        // todo also add 4 subdivisions within the tile the same way
        auto bb = bbox_union(start_bbox, Vector2i(from.local()), {}, own_size);
        if (path_search::is_passable(w, chunk_coords_{from}, bb, own_id, p))
        {
            indexes[{from, {}}] = 1;
            nodes.push_back({.dist = from_offset_len, .prev = 0, .coord = from, .offset = {}});
            Q.push_back(1);
            std::push_heap(Q.begin(), Q.end(), heap_comparator);
        }
    }

    while (!Q.empty())
    {
        fm_assert(nodes.size() < 500);

        std::pop_heap(Q.begin(), Q.end(), heap_comparator);
        const auto id = Q.back();
        Q.pop_back();

        fm_debug_assert(id < nodes.size());
        auto& node = nodes[id];
        fm_debug_assert(node.dist != (uint32_t)-1);

        Debug{} << "node" << id << "|" << node.coord.to_signed3() << node.offset << "|" << node.dist;

        const auto bb0 = bbox_from_pos(Vector2(node.coord.local()), node.offset, own_size);

        for (auto [vec, len] : directions)
        {
            auto [new_coord, new_offset] = object::normalize_coords(node.coord, node.offset, vec);
            const auto dist = node.dist + len;
            if (dist >= max_dist)
                continue;

            const auto sz = nodes.size();
            auto [it, created] = indexes.try_emplace({.coord = new_coord, .offset = new_offset}, sz);
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

            if (!created && dist >= node.dist)
                continue;
            node.dist = dist;

            auto e = make_edge({node.coord, node.offset}, {new_coord, new_offset});
            if (auto [it, created] = edges.try_emplace(e, edge_status::unknown); created)
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

            Debug{} << (created ? " new" : " old") << new_idx << "|" << node.coord.to_signed3() << node.offset << "|" << node.dist;

            Q.push_back(new_idx);
            std::push_heap(Q.begin(), Q.end(), heap_comparator);

        }
    }

    Debug {} << "done";

    // todo...
    return result;
}

} // namespace floormat
