#include "raycast-loop.hpp"
#include "grid-object.hpp"
#include "world.hpp"

namespace floormat::Raycast {

raycast_result_s raycast(world& w, point from, point to, object_id self,
                         Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius)
{
    return detail::do_raycasting<false, false>(nullptr, w, from, to, self, pass_grid_pool, nullptr, pred, nullptr,
                                               radius);
}

raycast_result_s raycast(world& w, point from, point to, object_id self,
                         Grid::Object::Pool& object_pool, Search::pred const& pred, float radius)
{
    return detail::do_raycasting<false, true>(nullptr, w, from, to, self, object_pool.pass_pool(), object_pool,
                                              pred, nullptr, radius);
}

raycast_result_s raycast(const world& w, point from, point to, object_id self,
                         const Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred,
                         Array<grid_order>& orders, float radius)
{
    return detail::do_raycasting<false, false>(nullptr, w, from, to, self, pass_grid_pool, nullptr, pred, &orders,
                                               radius);
}

raycast_result_s raycast(const world& w, point from, point to, object_id self,
                         const Grid::Object::Pool& object_pool, Search::pred const& pred,
                         Array<grid_order>& orders, float radius)
{
    return detail::do_raycasting<false, true>(nullptr, w, from, to, self, object_pool.pass_pool(), object_pool,
                                              pred, &orders, radius);
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                   Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius)
{
    return detail::do_raycasting<true, false>(diag, w, from, to, self, pass_grid_pool, nullptr, pred, nullptr,
                                              radius);
}

bool is_occluded(world& w, point from, point to, object_id self,
                 Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius)
{
    return !detail::do_raycasting<false, false, detail::raycast_mode::any_hit>(nullptr, w, from, to, self,
                                                                               pass_grid_pool, nullptr, pred,
                                                                               nullptr, radius).success;
}

bool is_occluded(const world& w, point from, point to, object_id self,
                 const Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred,
                 Array<grid_order>& orders, float radius)
{
    return !detail::do_raycasting<false, false, detail::raycast_mode::any_hit>(nullptr, w, from, to, self,
                                                                               pass_grid_pool, nullptr, pred,
                                                                               &orders, radius).success;
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                   Grid::Object::Pool& object_pool, Search::pred const& pred, float radius)
{
    return detail::do_raycasting<true, true>(diag, w, from, to, self, object_pool.pass_pool(), object_pool, pred,
                                             nullptr, radius);
}

} // namespace floormat::Raycast
