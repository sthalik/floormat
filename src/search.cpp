#include "search.hpp"
#include "search-bbox.hpp"
#include "search-astar.hpp"
#include "search-cache.hpp"
#include "global-coords.hpp"
#include "world.hpp"
#include "pass-mode.hpp"
#include "RTree-search.hpp"
#include "rect-intersects.hpp"
#include "compat/array-size.hpp"
#include "compat/function2.hpp"
#include <bit>

namespace floormat::Search {

namespace {
constexpr auto never_continue_1 = [](collision_data) constexpr { return path_search_continue::blocked; };
constexpr auto never_continue_ = pred{never_continue_1};
constexpr auto always_continue_1 = [](collision_data) constexpr { return path_search_continue::pass; };
constexpr auto always_continue_ = pred{always_continue_1};
} // namespace

const pred& never_continue() noexcept { return never_continue_; }
const pred& always_continue() noexcept { return always_continue_; }
//static_assert(1 << 2 == div_factor);

} // namespace floormat::Search

namespace floormat {

using namespace Search;

bool path_search::is_passable_1(chunk& c, Vector2 min, Vector2 max, object_id own_id, const pred& p)
{
    constexpr auto bbox_size = Vector2{0xff, 0xff};
    constexpr auto chunk_bounds = bbox<float>{
        -TILE_SIZE2/2 - bbox_size/2,
        TILE_MAX_DIM*TILE_SIZE2 - TILE_SIZE2/2 + bbox_size,
    };
    if (!rect_intersects(min, max, chunk_bounds.min, chunk_bounds.max))
        return true;

    auto& rt = *c.rtree();
    bool is_passable = true;
    rt.Search(min.data(), max.data(), [&](uint64_t data, const auto& r)
    {
        auto x = std::bit_cast<collision_data>(data);
        if (x.id != own_id && x.pass != (uint64_t)pass_mode::pass)
        {
            if (rect_intersects(min, max, {r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}))
                if (p(x) != path_search_continue::pass)
                {
                    is_passable = false;
                    //[[maybe_unused]] auto obj = c.world().find_object(x.data);
                    return false;
                }
        }
        return true;
    });
    return is_passable;
}

bool path_search::is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors,
                               Vector2 min, Vector2 max, object_id own_id, const pred& p)
{
    fm_debug_assert(max >= min);

    if (c0)
        // it's not correct to return true if c == nullptr
        // because neighbors can still contain bounding boxes for that tile
        if (!is_passable_1(*c0, min, max, own_id, p))
            return false;

    for (auto i = 0uz; i < 8; i++)
    {
        auto nb = world::neighbor_offsets[i];
        auto* c2 = neighbors[i];

        if (c2)
        {
            static_assert(array_size(world::neighbor_offsets) == 8);
            constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
            const auto off = Vector2(nb)*Vector2(chunk_size);
            const auto min_ = min - off, max_ = max - off;

            if (!is_passable_1(*c2, min_, max_, own_id, p))
                return false;
        }
    }

    return true;
}

bool path_search::is_passable(world& w, global_coords coord,
                              Vector2b offset, Vector2ui size_,
                              object_id own_id, const pred& p)
{
    auto center = iTILE_SIZE2 * Vector2i(coord.local()) + Vector2i(offset);
    auto size = Vector2(size_);
    auto min = Vector2(center) - size*.5f, max = min + size;
    return is_passable(w, coord, {min, max}, own_id, p);
}

bool path_search::is_passable(world& w, struct Search::cache& cache, global_coords coord,
                              Vector2b offset, Vector2ui size_,
                              object_id own_id, const pred& p)
{
    auto center = iTILE_SIZE2 * Vector2i(coord.local()) + Vector2i(offset);
    auto size = Vector2(size_);
    auto min = Vector2(center) - size*.5f, max = min + size;
    return is_passable(w, cache, coord, {min, max}, own_id, p);
}

bool path_search::is_passable(world& w, chunk_coords_ ch, const bbox<float>& bb,
                              object_id own_id, const pred& p)
{
    auto* c = w.at(ch);
    auto neighbors = w.neighbors(ch);
    return is_passable_(c, neighbors, bb.min, bb.max, own_id, p);
}

bool path_search::is_passable(world& w, struct Search::cache& cache, chunk_coords_ ch0,
                              const bbox<float>& bb, object_id own_id, const pred& p)
{
    auto* c = cache.try_get_chunk(w, ch0);
    auto nbs = cache.get_neighbors(w, ch0);
    return is_passable_(c, nbs, bb.min, bb.max, own_id, p);
}

} // namespace floormat
