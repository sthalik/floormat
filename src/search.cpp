#include "search.hpp"
#include "search-common.hpp"
#include "search-astar.hpp"
#include "search-cache.hpp"
#include "global-coords.hpp"
#include "world.hpp"
#include "pass-mode.hpp"
#include "object.hpp"
#include "RTree-search.hpp"
#include "rect-intersects.hpp"
#include "compat/array-size.hpp"
#include "compat/function2.hpp"
#include "point.inl"
#include <bit>
#include <mg/Range.h>

namespace floormat::Search {

using psc = path_search_continue;

namespace {
namespace detail {

template<typename Chunk>
constexpr inline auto never_continueʹʹ = [](Chunk&, collision_data, Range2D) constexpr { return psc::blocked; };

template<typename Chunk>
constexpr inline auto never_continueʹ = Pred<Chunk>{never_continueʹʹ<Chunk>};
template<typename Chunk>
constexpr inline auto always_continueʹʹ = [](Chunk&, collision_data, Range2D) constexpr { return psc::pass; };
template<typename Chunk>
constexpr inline auto always_continueʹ = Pred<Chunk>{always_continueʹʹ<Chunk>};

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
constexpr inline const auto octile_distanceʹ = heuristic{octile_distanceʹʹ};

template<typename Chunk>
constexpr inline auto without_crittersʹʹ = [](Chunk& self, collision_data data, Range2D) -> path_search_continue {
    // 'scenery' covers all object types here, including critters
    if (data.type == (uint64_t)collision_type::scenery)
    {
        auto obj = self.world().find_object(data.id);
        fm_assert(obj);
        if (obj->type() == object_type::critter)
            return path_search_continue::pass;
    }
    return path_search_continue::blocked;
};
template<typename Chunk> constexpr inline auto without_crittersʹ = Pred<Chunk>{without_crittersʹʹ<Chunk>};

template<typename Chunk>
bool is_passable_1(Chunk& c, Vector2 min, Vector2 max, const Pred<Chunk>& p)
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

template<typename Chunk>
bool is_passable_(Chunk* c0, const std::array<Chunk*, 8>& neighbors, Vector2 min, Vector2 max, const Pred<Chunk>& p)
{
    fm_debug_assert(max >= min);

    if (c0)
        // it's not correct to return true if c == nullptr
        // because neighbors can still contain bounding boxes for that tile
        if (!is_passable_1(*c0, min, max, p))
            return false;

    for (auto i = 0u; i < 8; i++)
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

template<typename World, typename Chunk>
bool is_passable(World& w, chunk_coords_ ch, const Range2D& bb, const Pred<Chunk>& p)
{
    Chunk* c = w.at(ch);
    auto neighbors = w.neighbors(ch);
    return is_passable_(c, neighbors, bb.min(), bb.max(), p);
}
} // namespace detail
} // namespace

bool is_passable_1(chunk& c, Vector2 min, Vector2 max, const Pred<chunk>& p)
{ return detail::is_passable_1(c, min, max, p); }
bool is_passable_1(const chunk& c, Vector2 min, Vector2 max, const Pred<const chunk>& p)
{ return detail::is_passable_1(c, min, max, p); }

bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max, const Pred<chunk>& p)
{ return detail::is_passable_(c0, neighbors, min, max, p); }
bool is_passable_(const chunk* c0, const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max, const Pred<const chunk>& p)
{ return detail::is_passable_(c0, neighbors, min, max, p); }

bool is_passable(world& w, chunk_coords_ ch, const Range2D& bb, const Pred<chunk>& p)
{ return detail::is_passable(w, ch, bb, p); }
bool is_passable(const world& w, chunk_coords_ ch, const Range2D& bb, const Pred<const chunk>& p)
{ return detail::is_passable(w, ch, bb, p); }

template<typename Chunk> const Pred<Chunk>& never_continue() noexcept { return detail::never_continueʹ<Chunk>; }
template<typename Chunk> const Pred<Chunk>& always_continue() noexcept { return detail::always_continueʹ<Chunk>; }
template<typename Chunk> const Pred<Chunk>& without_critters() noexcept { return detail::without_crittersʹ<Chunk>; }
const heuristic& octile_distance() noexcept { return detail::octile_distanceʹ; }

template const Pred<chunk>& never_continue() noexcept;
template const Pred<const chunk>& never_continue() noexcept;
template const Pred<chunk>& always_continue() noexcept;
template const Pred<const chunk>& always_continue() noexcept;
template const Pred<chunk>& without_critters() noexcept;
template const Pred<const chunk>& without_critters() noexcept;

} // namespace floormat::Search
