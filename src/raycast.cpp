#include "compat/nanosecond.hpp"
#include "compat/timer.hpp"
#include "raycast-loop.hpp"
#include "grid-object.hpp"
#include "world.hpp"

namespace floormat::rc {

namespace {

template<bool EnableDiagnostics, bool HasObjectPool>
raycast_result_s raycast(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                         world& w, point from, point to, object_id self, Grid::Pass::Pool& pass_pool,
                         std::conditional_t<HasObjectPool, Grid::Object::Pool&, std::nullptr_t> objects,
                         Search::pred const& pred, float radius)
{
    auto t = Time::now();
    auto ret = detail::do_raycasting<EnableDiagnostics, HasObjectPool>(diag, w, from, to, self, pass_pool, objects,
                                                                       pred, radius);
    ret.time = Time::to_seconds(t.update());
    return ret;
}

} // namespace

raycast_result_s raycast(world& w, point from, point to, object_id self,
                         Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius)
{
    return raycast<false, false>(nullptr, w, from, to, self, pass_grid_pool, nullptr, pred, radius);
}

raycast_result_s raycast(world& w, point from, point to, object_id self,
                         Grid::Object::Pool& object_pool, Search::pred const& pred, float radius)
{
    return raycast<false, true>(nullptr, w, from, to, self, object_pool.pass_pool(), object_pool, pred, radius);
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                   Grid::Pass::Pool& pass_grid_pool, Search::pred const& pred, float radius)
{
    return raycast<true, false>(diag, w, from, to, self, pass_grid_pool, nullptr, pred, radius);
}

raycast_result_s raycast_with_diag(raycast_diag_s& diag, world& w, point from, point to, object_id self,
                                   Grid::Object::Pool& object_pool, Search::pred const& pred, float radius)
{
    return raycast<true, true>(diag, w, from, to, self, object_pool.pass_pool(), object_pool, pred, radius);
}

} // namespace floormat::rc
