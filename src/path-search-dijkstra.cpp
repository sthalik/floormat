#include "path-search.hpp"
#include "object.hpp"
#include "compat/math.hpp"
#include <Corrade/Containers/PairStl.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Functions.h>

namespace floormat {

path_search_result path_search::Dijkstra(world& w, Vector2ub own_size, object_id own_id,
                                         global_coords from, Vector2b from_offset,
                                         global_coords to, Vector2b to_offset,
                                         const pred& p)
{
    fm_assert(from.x <= to.x && from.y <= to.y);
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
    astar.reserve(TILE_COUNT*(size_t)(subdivide_factor*subdivide_factor));

    static constexpr auto eps = (uint32_t)math::ceil(math::sqrt((Vector2(div_size)/4).product()));
    static_assert(eps > 1 && eps < TILE_SIZE2.x());

    const auto pos0 = Vector2(from.local()) * TILE_SIZE2;
    const auto start_bbox = bbox<float>{pos0 - Vector2(own_size/2), pos0 + Vector2(own_size)};
    const auto from_offset_f = Vector2(from_offset);
    const auto from_offset_len = Math::max(eps, (uint32_t)Math::ceil(from_offset_f.length()));

    struct tuple
    {
        bbox<float> bb;
        uint32_t dist;
    };

    constexpr auto get_bbox = [](chunk_coords_ ch_1, local_coords pos1, Vector2b off1,
                                 chunk_coords_ ch_2, local_coords pos2, Vector2b off2,
                                 Vector2ub size, uint32_t dist0) -> tuple
    {
        constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
        auto c = (Vector2i(ch_2.x, ch_2.y) - Vector2i(ch_1.x, ch_1.y)) * chunk_size;
        auto t = (Vector2i(pos2) - Vector2i(pos1)) * iTILE_SIZE2;
        auto o = Vector2i(off2) - Vector2i(off1);
        auto cto = Vector2i(c + t + o);
        auto dist = Math::max(1u, (uint32_t)Math::ceil(Vector2(cto).length()));
        auto center0 = Vector2i(pos1) * iTILE_SIZE2 + Vector2i(off1);
        auto min0 = center0 - Vector2i(size/2), max0 = min0 + Vector2i(size);
        auto min1 = min0 + cto, max1 = max0 + cto;
        fm_debug_assert(dist > eps);

        return {
            { .min = Vector2(Math::min(min0, min1)),
              .max = Vector2(Math::max(max0, max1)) },
            dist0 + dist,
        };
    };

    path_search_result result;
    fm_debug_assert(result._node); // todo
    auto& path = result._node->vec; path.clear();

    constexpr auto div = Vector2i(subdivide_factor);
    constexpr auto div_size = Vector2i(iTILE_SIZE2 / div);
    constexpr auto tile_start = Vector2i(iTILE_SIZE2/-2);

    astar.push(astar_edge{from, from_offset, from, from_offset}, 0);

    const auto ch0 = chunk_coords_{from};
    const auto bb0 = bbox_union(start_bbox, Vector2i(from.local()), {}, own_size);
    if (from_offset_len >= eps && is_passable(w, ch0, bb0, own_id, p))
        astar.push({from, from_offset, from, from_offset}, from_offset_len);

    struct pair { Vector2i dir; uint32_t len; };

    constexpr auto dirs = [] constexpr -> std::array<pair, 8> {
        constexpr auto len1 = path_search::div_size;
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
            vec *= path_search::div_size;
#if 0
        for (auto i = 0uz; i < array.size(); i++)
            for (auto j = 0uz; j < i; j++)
                fm_assert(array[i].dir != array[j].dir);
#endif
        return array;
    }();

    while (!astar.empty())
    {
        auto [cur, dist0] = astar.pop();
        if (!astar.add_visited(cur))
            continue;
        for (auto [dir, len] : dirs)
        {

        }
    }

    // todo...
    return result;
}

path_search_result path_search::Dijkstra(world& w, const object& obj, global_coords to, Vector2b to_offset, const pred& p)
{
    constexpr auto full_tile = Vector2ub(iTILE_SIZE2*3/4);
    auto size = Math::max(obj.bbox_size, full_tile);

    // todo fixme
    // if bbox_offset is added to obj's offset, then all coordinates in the paths are shifted by bbox_offset.
    // maybe add handling to subtract bbox_offset from the returned path.
    // for that it needs to be passed into callees separately.
    fm_assert(obj.bbox_offset.isZero());
    return Dijkstra(w, size, obj.id, obj.coord, obj.offset, to, to_offset, p);
}

} // namespace floormat
