#include "app.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include "src/tile.hpp"
#include "src/critter.hpp"
#include "src/grid-pass.hpp"
#include "src/grid-cover.hpp"

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
static_assert(has_subscript<Cover::Pool, chunk>);
static_assert(!has_subscript<Cover::Pool, const chunk>);

} // namespace
} // namespace floormat
