#include "search.hpp"
#include "search-common.hpp"
#include "search-astar.hpp"
#include "search-cache.hpp"
#include "global-coords.hpp"
#include "world.hpp"
#include "pass-mode.hpp"
#include "RTree-search.hpp"
#include "rect-intersects.hpp"
#include "compat/array-size.hpp"
#include "compat/function2.hpp"
#include "point.inl"
#include <bit>
#include <mg/Range.h>

namespace floormat::Search {

namespace {
constexpr auto never_continue_1 = [](chunk&, collision_data, Range2D) constexpr { return path_search_continue::blocked; };
constexpr auto never_continue_ = pred{never_continue_1};
constexpr auto always_continue_1 = [](chunk&, collision_data, Range2D) constexpr { return path_search_continue::pass; };
constexpr auto always_continue_ = pred{always_continue_1};

#if 0
constexpr auto euclidean_distanceʹʹ  = [](point cur, point goal) constexpr -> uint32_t { return (uint32_t)(cur - goal).length();  };
constexpr auto euclidean_distanceʹ = heuristic{euclidean_distanceʹʹ};
constexpr auto manhattan_distanceʹʹ  = [](point cur, point goal) constexpr -> uint32_t { return Vector2ui(Math::abs(cur - goal)).sum();  };
constexpr auto manhattan_distanceʹ = heuristic{manhattan_distanceʹʹ};
#endif

constexpr auto octile_distanceʹʹ = [](point cur, point goal) constexpr -> uint32_t {
    const auto d = Vector2ui{Math::abs(cur - goal)};
    const uint32_t dx = d.x();
    const uint32_t dy = d.y();
    const uint32_t mn = dx < dy ? dx : dy;
    const uint32_t mx = dx > dy ? dx : dy;
    // step costs must match search-astar.cpp directions[]
    constexpr uint32_t axial = (uint32_t)div_size.x();
    constexpr uint32_t diag  = (uint32_t)(div_size.length() + 1);
    return mx + (diag - axial) * mn / axial;
};
constexpr auto octile_distanceʹ = heuristic{octile_distanceʹʹ};

} // namespace

const pred& never_continue() noexcept { return never_continue_; }
const pred& always_continue() noexcept { return always_continue_; }

#if 0
const heuristic& euclidean_distance() noexcept { return euclidean_distanceʹ; }
const heuristic& manhattan_distance() noexcept { return manhattan_distanceʹ; }
#endif
const heuristic& octile_distance() noexcept { return octile_distanceʹ; }

} // namespace floormat::Search

namespace floormat::path_search {

bool is_passable_1(chunk& c, Vector2 min, Vector2 max, const pred& p)
{
    constexpr auto bbox_size = Vector2{0xff, 0xff};
    constexpr auto chunk_bounds = Range2D{
        -TILE_SIZE2/2 - bbox_size/2,
        TILE_MAX_DIM*TILE_SIZE2 - TILE_SIZE2/2 + bbox_size,
    };
    if (!rect_intersects(min, max, chunk_bounds.min(), chunk_bounds.max()))
        return true;

    bool ret = true;
    (void)is_passable_common(c, min, max, [&](uint64_t x, const Chunk_RTree::Rect& r) {
        auto data = std::bit_cast<collision_data>(x);
        if (data.pass == (uint64_t)pass_mode::pass)
            return true;
        auto rect = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
        if (!rect_intersects(min, max, rect.min(), rect.max()))
            return true;
        if (p(c, data, rect) == path_search_continue::pass)
            return true;
        return ret = false;
    });
    return ret;
}

bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors,
                               Vector2 min, Vector2 max, const pred& p)
{
    fm_debug_assert(max >= min);

    if (c0)
        // it's not correct to return true if c == nullptr
        // because neighbors can still contain bounding boxes for that tile
        if (!is_passable_1(*c0, min, max, p))
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

            if (!is_passable_1(*c2, min_, max_, p))
                return false;
        }
    }

    return true;
}

bool is_passable(world& w, chunk_coords_ ch, const Range2D& bb, const pred& p)
{
    auto* c = w.chunk_at_memo(ch);
    auto neighbors = w.neighbors(ch);
    return is_passable_(c, neighbors, bb.min(), bb.max(), p);
}

} // namespace floormat::path_search
