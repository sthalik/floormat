#pragma once
#include "search-pred.hpp"
//#include "pass-mode.hpp"
//#include "object-id.hpp"
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
    // In the line's frame.
    Range2D rect;
    bool has_collider;
};

// A box of bbox_size centered on each pixel of the line from first_step to last_step, and on both
// orthogonal points of each diagonal step. Pixels are in the frame of chunk ch, as for its rects. A
// pool must be built for a bbox of at least bbox_size. Its set bits are free unless p blocks an object of objects.
swept_collider_result find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                                          u32 first_step, u32 last_step, Vector2ui bbox_size,
                                          const Search::pred& p, Grid::Pass::Pool* pool = nullptr);
swept_collider_result find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                                          u32 first_step, u32 last_step, Vector2ui bbox_size,
                                          const Search::pred& p, Grid::Object::Pool& objects,
                                          Array<bool>& entry_blocked);
// Calls f for every collider blocking a step, in no particular order, until it returns false. A
// collider found in several cells comes once per cell, with a step it blocks there.
void find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                         u32 first_step, u32 last_step, Vector2ui bbox_size,
                         const Search::pred& p, Grid::Pass::Pool* pool,
                         function_view<bool(const swept_collider_result&) const> f);
void find_swept_collider(world& w, chunk_coords_ ch, const Raycast::bresenham_line& line,
                         u32 first_step, u32 last_step, Vector2ui bbox_size,
                         const Search::pred& p, Grid::Object::Pool& objects, Array<bool>& entry_blocked,
                         function_view<bool(const swept_collider_result&) const> f);

} // namespace floormat
