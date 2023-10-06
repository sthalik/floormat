#include "path-search.hpp"
#include "compat/math.hpp"
#include "object.hpp"
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

    const auto pos0 = Vector2(from.local()) * TILE_SIZE2;
    const auto start_bbox = bbox<float>{pos0 - Vector2(own_size/2), pos0 + Vector2(own_size)};

    constexpr auto div = Vector2ub{subdivide_factor};
    constexpr auto sub_size = Vector2i(Vector2ub(iTILE_SIZE2) / div);
    constexpr auto sqrt_2 = (float)math::sqrt(2);

    constexpr Vector2i directions[8] = {
        iTILE_SIZE2 * Vector2i{ -1,  0 },
        iTILE_SIZE2 * Vector2i{  0, -1 },
        iTILE_SIZE2 * Vector2i{  1,  0 },
        iTILE_SIZE2 * Vector2i{  0,  1 },
        iTILE_SIZE2 * Vector2i{  1,  1 },
        iTILE_SIZE2 * Vector2i{ -1, -1 },
        iTILE_SIZE2 * Vector2i{ -1,  1 },
        iTILE_SIZE2 * Vector2i{  1, -1 },
    };
    const auto bb0 = bbox_union(start_bbox, Vector2i(from.local()), {}, own_size);
    //if (is_passable(w, chunk_coords_{from}, bb0.min, bb0.max, own_id, p))
    //    push_heap(_astar.Q, {from, from_offset, from, {}, 0});

    path_search_result result;
    fm_debug_assert(result._node); // todo
    auto& path = result._node->vec; path.clear();

    {
        auto [pos2, _offset2] = object::normalize_coords(from, from_offset, {});
    }

    for (const auto& dir : directions)
    {
        auto pos = object::normalize_coords(from, from_offset, dir);
    }

    while (!astar.empty())
    {
    }

    auto [cmin, cmax] = Math::minmax(Vector2i(from.chunk()) - Vector2i(1, 1),
                                     Vector2i(to.chunk()) + Vector2i(1, 1));

    // todo...
    return result;
}

path_search_result path_search::Dijkstra(world& w, const object& obj,
                                         global_coords to, Vector2b to_offset,
                                         const pred& p)
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
