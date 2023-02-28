#include "rotation.inl"

namespace floormat {

/* N   0   -32    32  16
 * E   32   0     16  32
 * S   0    32    32  16
 * W  -32   0     16  32
 */

/* N   16  -32    32  16
 * E   32   16    16  32
 * S  -16   32    32  16
 * W  -32  -16    16  32
 */

using bbox = Pair<Vector2b, Vector2ub>;

static_assert(rotate_bbox({  16, -32 }, { 32, 16 },  rotation::N, rotation::E) == bbox{{ 32,  16}, {16, 32}});
static_assert(rotate_bbox({  16, -32 }, { 32, 16 },  rotation::N, rotation::S) == bbox{{-16,  32}, {32, 16}});
static_assert(rotate_bbox({  16, -32 }, { 32, 16 },  rotation::N, rotation::W) == bbox{{-32, -16}, {16, 32}});
static_assert(rotate_bbox({  32,  16 }, { 16, 32 },  rotation::E, rotation::S) == bbox{{-16,  32}, {32, 16}});
static_assert(rotate_bbox({  32,  16 }, { 16, 32 },  rotation::E, rotation::N) == bbox{{ 16, -32}, {32, 16}});
static_assert(rotate_bbox({ -32, -16 }, { 16, 32 },  rotation::W, rotation::S) == bbox{{-16,  32}, {32, 16}});

static_assert(rotate_bbox({ 1, 2 }, { 3, 4 }, rotation::E, rotation::E) == bbox{{1, 2}, {3, 4}});
static_assert(rotate_bbox({ 1, 2 }, { 3, 4 }, rotation::N, rotation::N) == bbox{{1, 2}, {3, 4}});

} // namespace floormat
