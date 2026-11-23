#pragma once
#include "raycast.hpp"
#include "tile-defs.hpp"
#include <array>
#include <cstddef>
#include <type_traits>
#include <mg/Range.h>

namespace floormat { struct grid_order; }
namespace floormat::Raycast { class bresenham_line; }
namespace floormat::Grid::Object { class Pool; class Grid; class ConstGrid; template<typename T> class cell_object_view; }

namespace floormat::Raycast::detail {

template<typename World>
struct line_cell
{
    // The 3x3 chunks around the cell's, row by row from (-1, -1). Any may be null.
    std::array<std::conditional_t<std::is_const_v<World>, const chunk, chunk>*, 9> chunks;
    // Offset of the cell's chunk from the line's chunk, in chunks.
    Vector2i chunk_offset;
    // The cell holds no pixel of the line other than those of steps first_step to last_step and
    // the orthogonal points of steps first_step - 1 to last_step.
    u32 first_step, last_step;
};

template<typename World>
using object_grid_type = std::conditional_t<std::is_const_v<World>, Grid::Object::ConstGrid, Grid::Object::Grid>;

// Called instead of visit for each corner of a skipped square where p blocks an object listed there.
template<typename World, typename T>
using object_cell_visitor = function_view<bool(const line_cell<World>&, const object_grid_type<World>&,
                                               Grid::Object::cell_object_view<T>, ArrayView<const bool>) const>;

// Visits the squares between 4 neighbouring nodes, or whole chunks when pool is nullptr, that the line's pixels
// from first_step to last_step cross. Skips a square whose corners are set and where p blocks no object.
template<typename World, typename Pool>
void traverse_line_cells(World& w, chunk_coords_ ch, const bresenham_line& line,
                         u32 first_step, u32 last_step, bool both_orthogonal,
                         Pool pool, const Search::pred& p,
                         std::array<Array<bool>, 4>* entry_blocked, Array<grid_order>* orders,
                         function_view<bool(const line_cell<World>&) const> visit,
                         object_cell_visitor<World, u16> visit_objects_u16,
                         object_cell_visitor<World, u32> visit_objects_u32);

struct aabb_result
{
    float tmin;
    bool result;
};

// Defined in raycast-loop.cpp, which GCC and Clang compile without -ffinite-math-only, so a zero component gives
// ±inf there. Under cl it gives ±1e20.
Vector2 dir_inverse(Vector2 dir);
std::array<u8, 2> ray_aabb_signs(Vector2 ray_dir_inv_norm);
aabb_result ray_aabb_intersection(Vector2 ray_origin, Vector2 ray_dir_inv_norm,
                                  std::array<Vector2, 2> box_minmax, std::array<u8, 2> signs);
// ray_dir must be unit length.
aabb_result ray_rounded_rect_intersection(Vector2 ray_origin, Vector2 ray_dir, Vector2 ray_dir_inv_norm,
                                          std::array<Vector2, 2> box_minmax, float radius,
                                          std::array<u8, 2> signs);

template<typename T>
constexpr bool within_chunk_bounds(Math::Vector2<T> p0, Math::Vector2<T> p1)
{
    constexpr auto b = chunk_collision_bounds<Math::Range2D<T>>;
    return b.min().x() <= p1.x() && b.max().x() >= p0.x() &&
           b.min().y() <= p1.y() && b.max().y() >= p0.y();
}

// any_hit returns the first collider found, which may not be the nearest.
enum class raycast_mode : u8 { closest_hit, any_hit, };

// A const World makes it read only built grids and append an order to orders for each chunk without one.
template<bool EnableDiagnostics, bool HasObjectPool, raycast_mode Mode = raycast_mode::closest_hit, typename World>
raycast_result_s do_raycasting(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                               World& w, point from, point to, object_id self,
                               std::conditional_t<std::is_const_v<World>, const Grid::Pass::Pool&,
                                                  Grid::Pass::Pool&> pass_pool,
                               std::conditional_t<!HasObjectPool, std::nullptr_t,
                                                  std::conditional_t<std::is_const_v<World>, const Grid::Object::Pool&,
                                                                     Grid::Object::Pool&>> objects,
                               const Search::pred& pred, Array<grid_order>* orders, float radius);

} // namespace floormat::Raycast::detail
