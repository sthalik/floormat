#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile.hpp"
#include "src/critter.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"
#include "src/grid-object.hpp"
#include "src/grid-registry.hpp"
#include "src/grid-order.hpp"
#include "src/raycast.hpp"
#include "src/raycast-line.hpp"
#include "src/sweep-aabb.hpp"
#include "compat/function-view.hpp"
#include <concepts>

namespace floormat {
namespace {

// static_assert(!requires { ... }) is ill-formed outside a template when the expression is invalid
template<typename T, typename Arg> concept has_subscript = requires(T& t, Arg& a) { t[a]; };

template<typename T, typename U> constexpr inline bool same = std::is_same_v<T, U>;

using W = const world&;
using T = const const_tile_ref&;

static_assert(same<decltype(std::declval<W>().at(chunk_coords_{})), const chunk*>);
static_assert(same<decltype(std::declval<W>().neighbors(chunk_coords_{})), std::array<const chunk*, 8>>);
static_assert(has_subscript<world, chunk_coords_>);
static_assert(!has_subscript<const world, chunk_coords_>);
static_assert(same<decltype(std::declval<W>().find_object(object_id{})), const object*>);
static_assert(same<decltype(std::declval<W>().find_object<critter>(object_id{})), const critter*>);

static_assert(same<decltype(std::declval<T>().ground_atlas()), const ground_atlas*>);
static_assert(same<decltype(std::declval<T>().wall_north_atlas()), const wall_atlas*>);
static_assert(same<decltype(std::declval<T>().ground()), const_tile_image_ref>);
static_assert(same<decltype(std::declval<T>().wall_west()), const_wall_image_ref>);

static_assert(has_subscript<Pass::Pool, chunk>);
static_assert(!has_subscript<Pass::Pool, const chunk>);
static_assert(has_subscript<Grid::Cover::Pool, chunk>);
static_assert(!has_subscript<Grid::Cover::Pool, const chunk>);
static_assert(has_subscript<Grid::Object::Pool, chunk>);
static_assert(!has_subscript<Grid::Object::Pool, const chunk>);
static_assert(!has_subscript<const Pass::Pool, chunk>);
static_assert(!has_subscript<const Grid::Cover::Pool, chunk>);
static_assert(!has_subscript<const Grid::Object::Pool, chunk>);

template<typename P, typename G> concept has_find_built_grid = requires(const P& p, const chunk& c) {
    { p.find_built_grid(c) } -> std::same_as<G>;
};

static_assert(has_find_built_grid<Pass::Pool, Pass::ConstGrid>);
static_assert(has_find_built_grid<Grid::Object::Pool, Grid::Object::ConstGrid>);
static_assert(has_find_built_grid<Grid::Cover::Pool, Grid::Cover::ConstGrid>);

template<typename G> concept has_build_if_stale = requires(G& g) { g.build_if_stale(); };

template<typename G, typename ConstG> constexpr inline bool is_read_only_handle =
    std::is_default_constructible_v<ConstG> && std::is_copy_constructible_v<ConstG> &&
    std::is_copy_assignable_v<ConstG> &&
    std::is_constructible_v<bool, ConstG> && !std::is_convertible_v<ConstG, bool> &&
    !std::is_constructible_v<G, ConstG> && !std::is_constructible_v<G, const ConstG&> &&
    !has_build_if_stale<ConstG>;

static_assert(is_read_only_handle<Pass::Grid, Pass::ConstGrid>);
static_assert(is_read_only_handle<Grid::Object::Grid, Grid::Object::ConstGrid>);
static_assert(is_read_only_handle<Grid::Cover::Grid, Grid::Cover::ConstGrid>);

static_assert(same<decltype(std::declval<const Pass::ConstGrid&>().bits()), ArrayView<const usize>>);
static_assert(same<decltype(std::declval<const Grid::Object::Pool&>().pass_pool()), const Pass::Pool&>);
static_assert(same<decltype(std::declval<Grid::Object::Pool&>().pass_pool()), Pass::Pool&>);

template<typename R> concept has_pass_pool = requires(R& r, const Pass::Params& p) { r.pass_pool(p); };
template<typename R> concept has_object_pool = requires(R& r, const Grid::Object::Params& p) { r.object_pool(p); };
template<typename R> concept has_cover_pool = requires(R& r, const Grid::Cover::Params& p) { r.cover_pool(p); };
template<typename R>
concept has_pool_lookups = requires(R& r, const Pass::Params& p, const Grid::Cover::Params& cp) {
    { r.find_pass_pool(p) } -> std::same_as<const Pass::Pool*>;
    { r.find_object_pool(p) } -> std::same_as<const Grid::Object::Pool*>;
    { r.find_cover_pool(cp) } -> std::same_as<const Grid::Cover::Pool*>;
};

static_assert(has_pass_pool<Grid::GridRegistry>);
static_assert(has_object_pool<Grid::GridRegistry>);
static_assert(has_cover_pool<Grid::GridRegistry>);
static_assert(!has_pass_pool<const Grid::GridRegistry>);
static_assert(!has_object_pool<const Grid::GridRegistry>);
static_assert(!has_cover_pool<const Grid::GridRegistry>);
static_assert(has_pool_lookups<const Grid::GridRegistry>);

template<typename O> concept has_c = requires(O& o) { o.c; };

static_assert(same<decltype(std::declval<const object&>().chunk()), const chunk&>);
static_assert(same<decltype(std::declval<const object&>().world()), const world&>);
static_assert(!has_c<object>);
static_assert(!has_c<const object>);

static_assert(same<decltype(std::declval<const propagate_const<chunk*>&>().get()), const chunk*>);
static_assert(same<decltype(std::declval<propagate_const<chunk*>&>().get()), chunk*>);
static_assert(!std::is_copy_constructible_v<propagate_const<chunk*>>);
static_assert(!std::is_copy_assignable_v<propagate_const<chunk*>>);

template<typename O> concept has_parent = requires(O& o) { o.parent; };

static_assert(!has_parent<object>);

template<typename S> concept has_ptr = requires(S& s, const object& e) { s.ptr(e, 0uz); };

static_assert(has_ptr<object_storage>);
static_assert(!has_ptr<const object_storage>);

template<typename C> concept has_static_mesh = requires(C& c) { c.scenery_static_mesh; };

static_assert(!has_static_mesh<chunk>);
static_assert(!has_static_mesh<const chunk>);

template<typename C> concept has_pool_find = requires(const Grid::detail::PoolBase& p, C& c) { p.find(c); };

static_assert(has_pool_find<chunk>);
static_assert(has_pool_find<const chunk>);
static_assert(same<decltype(std::declval<const Grid::detail::PoolBase&>().find(std::declval<const chunk&>())),
                   const Grid::detail::GridBase*>);
static_assert(same<decltype(std::declval<const Grid::detail::PoolBase&>().find(std::declval<chunk&>())),
                   Grid::detail::GridBase*>);

template<typename Wo, typename P> concept is_raycast_callable =
    requires(Wo& w, P& pool, point pt, const Search::pred& p) { raycast(w, pt, pt, object_id{}, pool, p); };
template<typename Wo, typename P> concept is_read_only_raycast_callable =
    requires(Wo& w, P& pool, point pt, const Search::pred& p, Array<grid_order>& orders) {
        { raycast(w, pt, pt, object_id{}, pool, p, orders) } -> std::same_as<Raycast::raycast_result_s>;
    };

static_assert(is_read_only_raycast_callable<const world, const Pass::Pool>);
static_assert(is_read_only_raycast_callable<const world, const Grid::Object::Pool>);
static_assert(is_raycast_callable<world, Pass::Pool>);
static_assert(is_raycast_callable<world, Grid::Object::Pool>);
static_assert(!is_raycast_callable<const world, Pass::Pool>);
static_assert(!is_raycast_callable<const world, Grid::Object::Pool>);
static_assert(!is_raycast_callable<world, const Pass::Pool>);

using swept_visitor = function_view<bool(const swept_collider_result&) const>;
using entry_blocked_arrays = std::array<Array<bool>, 4>;

template<typename Wo, typename P>
concept is_read_only_swept_collider_callable =
    requires(Wo& w, P* pool, const Raycast::bresenham_line& l, const Search::pred& p, Array<grid_order>& orders,
             swept_visitor f) {
        { find_swept_collider(w, chunk_coords_{}, l, 0, 1, Vector2ui{}, p, orders, pool) }
            -> std::same_as<swept_collider_result>;
        { find_swept_collider(w, chunk_coords_{}, l, 0, 1, Vector2ui{}, p, orders) }
            -> std::same_as<swept_collider_result>;
        find_swept_collider(w, chunk_coords_{}, l, 0, 1, Vector2ui{}, p, pool, orders, f);
    };
template<typename Wo, typename P>
concept is_read_only_swept_object_collider_callable =
    requires(Wo& w, P& objects, const Raycast::bresenham_line& l, const Search::pred& p, Array<grid_order>& orders,
             entry_blocked_arrays& entry_blocked, swept_visitor f) {
        { find_swept_collider(w, chunk_coords_{}, l, 0, 1, Vector2ui{}, p, objects, orders, entry_blocked) }
            -> std::same_as<swept_collider_result>;
        find_swept_collider(w, chunk_coords_{}, l, 0, 1, Vector2ui{}, p, objects, orders, entry_blocked, f);
    };
template<typename Wo>
concept is_swept_collider_callable = requires(Wo& w, const Raycast::bresenham_line& l, const Search::pred& p) {
    find_swept_collider(w, chunk_coords_{}, l, 0, 1, Vector2ui{}, p);
};

static_assert(is_read_only_swept_collider_callable<const world, const Pass::Pool>);
static_assert(is_read_only_swept_object_collider_callable<const world, const Grid::Object::Pool>);
static_assert(is_swept_collider_callable<world>);
static_assert(!is_swept_collider_callable<const world>);

} // namespace
} // namespace floormat
