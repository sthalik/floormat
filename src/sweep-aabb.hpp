#pragma once
#include "search-pred.hpp"
//#include "pass-mode.hpp"
//#include "object-id.hpp"
#include <mg/Range.h>

namespace Magnum { using Range2D = Math::Range2D<Float>; }
namespace Magnum::Math { template<UnsignedInt dimensions, class T> class Range; }
namespace floormat { class chunk; class world; struct chunk_coords_; }
namespace floormat::rc { class bresenham_line; }
namespace floormat::Grid::Pass { class Pool; }

namespace floormat {

struct swept_collider_result
{
    // The first step the box can't reach, or last_step + 1.
    uint32_t step;
    collision_data collider;
    // In the line's frame.
    Range2D rect;
    bool has_collider;
};

// A box of bbox_size centered on each pixel of the line from first_step to last_step, and on both
// off-axis pixels of each diagonal step. Pixels are in the frame of chunk ch, as for its rects. A
// pool must be built for a bbox of at least bbox_size; its set bits are taken as free.
swept_collider_result find_swept_collider(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                                          uint32_t first_step, uint32_t last_step, Vector2ui bbox_size,
                                          const Search::pred& p, Grid::Pass::Pool* pool = nullptr);
// Calls f for every collider blocking a step, in no particular order, until it returns false. A
// collider found in several cells comes once per cell, with a step it blocks there.
void find_swept_collider(world& w, chunk_coords_ ch, const rc::bresenham_line& line,
                         uint32_t first_step, uint32_t last_step, Vector2ui bbox_size,
                         const Search::pred& p, Grid::Pass::Pool* pool,
                         fu2::function_view<bool(const swept_collider_result&) const> f);

} // namespace floormat
