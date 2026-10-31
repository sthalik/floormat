#include "raycast-loop.hpp"
#include "world.hpp"
#include "nanosecond.hpp"
#include "timer.hpp"

namespace floormat::rc {

raycast_result_s raycast(world& w, point from, point to, object_id self,
                         Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius)
{
    auto t = Time::now();
    auto ret = detail::do_raycasting<false>(nullptr, w, from, to, self, pass_grid_pool, pred, radius);
    ret.time = Time::to_seconds(t.update());
    return ret;
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                   Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius)
{
    auto t = Time::now();
    auto ret = detail::do_raycasting<true>(diag, w, from, to, self, pass_grid_pool, pred, radius);
    ret.time = Time::to_seconds(t.update());
    return ret;
}

} // namespace floormat::rc
