#pragma once
#include "raycast.hpp"
#include "tile-defs.hpp"
#include <array>
#include <cstddef>
#include <type_traits>
#include <mg/Range.h>

namespace floormat::rc { class bresenham_line; }

namespace floormat::rc::detail {

struct line_cell
{
    // The 3x3 chunks around the cell's, row by row from (-1, -1). Any may be null.
    std::array<chunk*, 9> chunks;
    // From the line's chunk.
    Vector2i chunk_offset;
    // The cell holds no pixel of the line other than those of steps first_step to last_step and
    // the off-axis pixels of steps first_step - 1 to last_step.
    uint32_t first_step, last_step;
};

// Visits the cells holding the line's pixels from first_step to last_step in step order, until
// visit returns false. Pixels are in the frame of chunk ch, as for its rects. With a pool, cells
// are the pool's, and those whose bit is set are skipped; without one, cells are chunks. Off-axis
// pixels count when the segment crosses them, or always with both_off_axis.
void traverse_line_cells(world& w, chunk_coords_ ch, const bresenham_line& line,
                         uint32_t first_step, uint32_t last_step, bool both_off_axis,
                         Grid::Pass::Pool* pool, const Search::pred& pred,
                         fu2::function_view<bool(const line_cell&) const> visit);

struct aabb_result
{
    float tmin;
    bool result;
};

// Defined in the .cpp, which honors inf, so a finite-math caller still gets ±inf.
Vector2 dir_inverse(Vector2 dir);
std::array<uint8_t, 2> ray_aabb_signs(Vector2 ray_dir_inv_norm);
aabb_result ray_aabb_intersection(Vector2 ray_origin, Vector2 ray_dir_inv_norm,
                                  std::array<Vector2, 2> box_minmax, std::array<uint8_t, 2> signs);
// ray_dir must be unit length.
aabb_result ray_rounded_rect_intersection(Vector2 ray_origin, Vector2 ray_dir, Vector2 ray_dir_inv_norm,
                                          std::array<Vector2, 2> box_minmax, float radius,
                                          std::array<uint8_t, 2> signs);

template<typename T>
constexpr bool within_chunk_bounds(Math::Vector2<T> p0, Math::Vector2<T> p1)
{
    constexpr auto b = chunk_collision_bounds<Math::Range2D<T>>;
    return b.min().x() <= p1.x() && b.max().x() >= p0.x() &&
           b.min().y() <= p1.y() && b.max().y() >= p0.y();
}

template<bool EnableDiagnostics>
raycast_result_s do_raycasting(std::conditional_t<EnableDiagnostics, raycast_diag_s&, std::nullptr_t> diag,
                               world& w, point from, point to, object_id self,
                               Grid::Pass::Pool& pool, const Search::pred& pred, float radius);

} // namespace floormat::rc::detail
