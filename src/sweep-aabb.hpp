#pragma once
#include "search-pred.hpp"
//#include "pass-mode.hpp"
//#include "object-id.hpp"
#include <array>
#include <mg/Range.h>

namespace Magnum { using Range2D = Math::Range2D<Float>; }
namespace Magnum::Math { template<UnsignedInt dimensions, class T> class Range; }
namespace floormat { class chunk; class world; struct chunk_coords_; }
namespace floormat::Raycast { class bresenham_line; }
namespace floormat::Grid::Pass { class Pool; }
namespace floormat::Grid::Object { class Pool; }

namespace floormat {

struct swept_collider_result
{
    // The first step the box can't reach, or last_step + 1.
    u32 step;
    collision_data collider;
    // The collider's rect, in the line's frame.
    Range2D rect;
    bool has_collider;
};

// Returns the first step whose box of bbox_size, at its point or an orthogonal point of the move into it, overlaps
// a collider p blocks. The line is in chunk ch's frame. pool must have min_pass == pass and a box at least as big.
swept_collider_result find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                                          u32 first_step, u32 last_step, Vector2ui bbox_size,
                                          const Search::pred& p, Grid::Pass::Pool* pool = nullptr);
swept_collider_result find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                                          u32 first_step, u32 last_step, Vector2ui bbox_size,
                                          const Search::pred& p, Grid::Object::Pool& objects,
                                          std::array<Array<bool>, 4>& entry_blocked);
// Calls f for every collider blocking a step, in no particular order, until it returns false.
// f can get one collider more than once, each time with a step it blocks.
void find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                         u32 first_step, u32 last_step, Vector2ui bbox_size,
                         const Search::pred& p, Grid::Pass::Pool* pool,
                         function_view<bool(const swept_collider_result&) const> f);
void find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                         u32 first_step, u32 last_step, Vector2ui bbox_size,
                         const Search::pred& p, Grid::Object::Pool& objects, std::array<Array<bool>, 4>& entry_blocked,
                         function_view<bool(const swept_collider_result&) const> f);

} // namespace floormat
