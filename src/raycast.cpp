#include "raycast-loop.hpp"
#include "world.hpp"
#include <mg/Timeline.h>

namespace floormat::rc {

raycast_result_s raycast(world& w, point from, point to, object_id self,
                         Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred)
{
    Timeline timeline;
    timeline.start();
    auto ret = detail::do_raycasting<false>(nullptr, w, from, to, self, pass_grid_pool, pred);
    ret.time = timeline.currentFrameDuration();
    return ret;
}

raycast_result_s raycast(world& w, point from, point to, object_id self)
{
    return raycast(w, from, to, self, w.raycast_pass_pool(), Search::never_continue());
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                   Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred)
{
    Timeline timeline;
    timeline.start();
    auto ret = detail::do_raycasting<true>(diag, w, from, to, self, pass_grid_pool, pred);
    ret.time = timeline.currentFrameDuration();
    return ret;
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self)
{
    return raycast_with_diag(diag, w, from, to, self, w.raycast_pass_pool(), Search::never_continue());
}

} // namespace floormat::rc
