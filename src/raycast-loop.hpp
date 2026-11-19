#pragma once
#include "raycast.hpp"
#include "tile-defs.hpp"
#include <array>
#include <cstddef>
#include <type_traits>
#include <mg/Range.h>

namespace floormat::Raycast { class bresenham_line; }
namespace floormat::Grid::Object { class Pool; class Grid; template<typename T> class cell_object_view; }

namespace floormat::Raycast::detail {

struct line_cell
{
    // The 3x3 chunks around the cell's, row by row from (-1, -1). Any may be null.
    std::array<chunk*, 9> chunks;
    // Offset of the cell's chunk from the line's chunk, in chunks.
    Vector2i chunk_offset;
    // The cell holds no pixel of the line other than those of steps first_step to last_step and
    // the orthogonal points of steps first_step - 1 to last_step.
    u32 first_step, last_step;
};

// Called instead of visit for each corner of a skipped square where p blocks an object listed there.
template<typename T>
using object_cell_visitor = function_view<bool(const line_cell&, const Grid::Object::Grid&,
                                               Grid::Object::cell_object_view<T>, ArrayView<const bool>) const>;

// Visits the squares between 4 neighbouring nodes, or whole chunks when pool is nullptr, that the line's pixels
// from first_step to last_step cross. Skips a square whose corners are set and where p blocks no object.
template<typename Pool>
void traverse_line_cells(world& w, chunk_coords_ ch, const bresenham_line& line,
                         u32 first_step, u32 last_step, bool both_orthogonal,
                         Pool pool, const Search::pred& p, std::array<Array<bool>, 4>* entry_blocked,
                         function_view<bool(const line_cell&) const> visit,
                         object_cell_visitor<u16> visit_objects_u16, object_cell_visitor<u32> visit_objects_u32);

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

template<bool EnableDiagnostics, bool HasObjectPool>
raycast_result_s do_raycasting(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                               world& w, point from, point to, object_id self, Grid::Pass::Pool& pass_pool,
                               std::conditional_t<HasObjectPool, Grid::Object::Pool&, std::nullptr_t> objects,
                               const Search::pred& pred, float radius);

} // namespace floormat::Raycast::detail
