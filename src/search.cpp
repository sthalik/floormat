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
#include "point.inl"
#include "compat/array-size.hpp"
#include "compat/function-view.hpp"
#include "compat/zip.hpp"
#include <bit>
#include <mg/Range.h>

namespace floormat::Search {

namespace {
namespace detail {

template<typename Chunk, typename F>
bool is_passable_1(Chunk& c, Vector2 min, Vector2 max, const F& f)
{
    constexpr auto bounds = chunk_collision_bounds<Range2D>;
    if (!rect_intersects(min, max, bounds.min(), bounds.max()))
        return true;

    bool ret = true;
    (void)is_passable_common(c, min, max, [&](u64 x, const Chunk_RTree::Rect& r) {
        auto data = std::bit_cast<collision_data>(x);
        // hole markers are stored in hole_rtree()
        fm_debug3_assert(data.type != (u64)collision_type::none);
        if (data.pass == (u64)pass_mode::pass)
            return true;
        auto rect = Range2D{{r.m_min[0], r.m_min[1]}, {r.m_max[0], r.m_max[1]}};
        if (!rect_intersects(min, max, rect.min(), rect.max()))
            return true;
        if constexpr (std::is_invocable_v<const F&, Chunk&, collision_data>)
        {
            if (f(c, data) == path_search_continue::pass)
                return true;
        }
        else if (f(c, data, rect) == path_search_continue::pass)
            return true;
        return ret = false;
    });
    return ret;
}

template<typename Chunk, typename F>
bool is_passable_neighbors_(const std::array<Chunk*, 8>& neighbors, Vector2 min, Vector2 max, const F& f)
{
    fm_debug3_assert(max >= min);

    for (const auto& [c2, nb] : zip(neighbors, world::neighbor_offsets))
    {
        if (c2)
        {
            static_assert(array_size(world::neighbor_offsets) == 8);
            const auto off = Vector2(nb)*chunk_size<Vector2>;
            const auto min_ = min - off, max_ = max - off;

            if (!is_passable_1(*c2, min_, max_, f))
                return false;
        }
    }
    return true;
}

template<typename Chunk, typename F>
bool is_passable_(Chunk* c0, const std::array<Chunk*, 8>& neighbors, Vector2 min, Vector2 max, const F& f)
{
    fm_debug3_assert(max >= min);

    if (c0)
        // it's not correct to return true if c == nullptr
        // because neighbors can still contain bounding boxes for that tile
        if (!is_passable_1(*c0, min, max, f))
            return false;

    return is_passable_neighbors_(neighbors, min, max, f);
}

template<typename World>
bool is_passable(World& w, chunk_coords_ ch, const Range2D& bb, const pred& p)
{
    auto* c = w.at(ch);
    auto neighbors = w.neighbors(ch);
    return is_passable_(c, neighbors, bb.min(), bb.max(), p);
}

} // namespace detail
} // namespace

bool is_passable_1(chunk& c, Vector2 min, Vector2 max, const pred& p)
{ return detail::is_passable_1(c, min, max, p); }
bool is_passable_1(const chunk& c, Vector2 min, Vector2 max, const pred& p)
{ return detail::is_passable_1(c, min, max, p); }

bool is_passable_(chunk* c0, const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max, const pred& p)
{ return detail::is_passable_(c0, neighbors, min, max, p); }
bool is_passable_(const chunk* c0, const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max, const pred& p)
{ return detail::is_passable_(c0, neighbors, min, max, p); }

bool is_passable_neighbors_(const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max, const pred& p)
{ return detail::is_passable_neighbors_(neighbors, min, max, p); }
bool is_passable_neighbors_(const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max, const pred& p)
{ return detail::is_passable_neighbors_(neighbors, min, max, p); }

bool is_passable(world& w, chunk_coords_ ch, const Range2D& bb, const pred& p)
{ return detail::is_passable(w, ch, bb, p); }
bool is_passable(const world& w, chunk_coords_ ch, const Range2D& bb, const pred& p)
{ return detail::is_passable(w, ch, bb, p); }

bool visit_colliders(chunk* c0, const std::array<chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                     const collider_visitor& f)
{ return detail::is_passable_(c0, neighbors, min, max, f); }
bool visit_colliders(const chunk* c0, const std::array<const chunk*, 8>& neighbors, Vector2 min, Vector2 max,
                     const collider_visitor& f)
{ return detail::is_passable_(c0, neighbors, min, max, f); }

path_search_continue without_critters(const chunk& self, collision_data data)
{
    // 'scenery' covers all object types here, including critters
    if (data.type == (u64)collision_type::scenery)
    {
        auto obj = self.world().find_object(data.id);
        fm_assert(obj);
        if (obj->type() == object_type::critter)
            return path_search_continue::pass;
    }
    return path_search_continue::blocked;
}

path_search_continue without_dynamic_objects(const chunk& self, collision_data data)
{
    return is_dynamic_object(self, data) ? path_search_continue::pass : path_search_continue::blocked;
}

bool is_dynamic_object(const object& o)
{
    return o.type() == object_type::critter;
}

bool is_dynamic_object(const chunk& c, collision_data x)
{
    if (x.type != (u64)collision_type::scenery)
        return false;
    const auto* obj = c.world().find_object(x.id);
    fm_assert(obj);
    return is_dynamic_object(*obj);
}

} // namespace floormat::Search
