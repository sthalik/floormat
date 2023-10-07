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
constexpr auto div = Vector2i(path_search::subdivide_factor);
constexpr auto div_size = path_search::div_size;
constexpr auto min_size = path_search::min_size;
constexpr auto tile_start = Vector2i(iTILE_SIZE2/-2);
constexpr auto inf =- (uint32_t)-1;

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

constexpr auto get_bbox(chunk_coords_ ch_1, local_coords pos1, Vector2b off1,
                        chunk_coords_ ch_2, local_coords pos2, Vector2b off2,
                        Vector2ub size, uint32_t dist0)
{
    auto c = (Vector2i(ch_2.x, ch_2.y) - Vector2i(ch_1.x, ch_1.y)) * chunk_size;
    auto t = (Vector2i(pos2) - Vector2i(pos1)) * iTILE_SIZE2;
    auto o = Vector2i(off2) - Vector2i(off1);
    auto cto = Vector2i(c + t + o);
    auto dist = Math::max(1u, (uint32_t)Math::ceil(Vector2(cto).length()));
    auto center0 = Vector2i(pos1) * iTILE_SIZE2 + Vector2i(off1);
    auto min0 = center0 - Vector2i(size/2u), max0 = min0 + Vector2i(size);
    auto min1 = min0 + cto, max1 = max0 + cto;

    return Pair<bbox<float>, uint32_t>{
        { .min = Vector2(Math::min(min0, min1)),
          .max = Vector2(Math::max(max0, max1)) },
        dist0 + dist,
    };
};

constexpr auto dirs = [] constexpr
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

size_t astar::hash_op::operator()(position coord) const
{
    static_assert(sizeof(global_coords) == 8);
    if constexpr(sizeof nullptr > 4)
        return fnvhash_64(&coord, sizeof coord);
    else
        return fnvhash_32(&coord, sizeof coord);
}

path_search_result path_search::Dijkstra(world& w, Vector2ub own_size, object_id own_id,
                                         global_coords from, Vector2b from_offset,
                                         global_coords to, Vector2b to_offset,
                                         const pred& p)
{
    auto heap_comparator = [&A = astar](uint32_t a, uint32_t b) {
        fm_debug_assert(std::max(a, b) < A.nodes.size());
        const auto& n1 = A.nodes[a];
        const auto& n2 = A.nodes[b];
        return n2.dist < n1.dist;
    };

    own_size = Math::max(own_size, Vector2ub(min_size));

    if (from.z() != to.z()) [[unlikely]]
        return {};

    // todo try removing this eventually
    if (from.z() != 0) [[unlikely]]
        return {};

    if (!is_passable(w, from, from_offset, own_size, own_id, p))
        return {};

    if (!is_passable(w, to, to_offset, own_size, own_id, p))
        return {};

    astar.clear();
    fm_debug_assert(astar.nodes.empty());

    const auto start_bbox = bbox_from_pos(Vector2(from.local()), from_offset, own_size);
    const auto from_offset_len = Math::max(1u, (uint32_t)Math::ceil(Vector2(from_offset).length()));

    path_search_result result;
    fm_debug_assert(result._node); // todo
    auto& path = result._node->vec; path.clear();

    astar.indexes[{from, from_offset}] = 0;
    astar.nodes.push_back({.dist = 0, .coord = from, .offset = from_offset });
    astar.Q.push_back(0);
    std::push_heap(astar.Q.begin(), astar.Q.end(), heap_comparator);

    if (!from_offset.isZero())
    {
        auto bb = bbox_union(start_bbox, Vector2i(from.local()), {}, own_size);
        if (is_passable(w, chunk_coords_{from}, bb, own_id, p))
        {
            astar.indexes[{from, {}}] = 1;
            astar.nodes.push_back({.dist = from_offset_len, .prev = 0, .coord = from, .offset = {}});
            astar.Q.push_back(1);
            std::push_heap(astar.Q.begin(), astar.Q.end(), heap_comparator);
        }
    }



    // todo...
    return result;
}

path_search_result path_search::Dijkstra(world& w, const object& obj,
                                         global_coords to, Vector2b to_offset,
                                         const pred& p)
{
    fm_assert(obj.bbox_offset.isZero());
    return Dijkstra(w, obj.bbox_size, obj.id, obj.coord, obj.offset, to, to_offset, p);
}

} // namespace floormat
