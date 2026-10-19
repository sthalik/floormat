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

using offsets = Pair<Vector2b, Vector2b>;

static_assert(rotate_offset({   0, -32 }, {}, rotation::N, rotation::S) == offsets{{  0,  31}, {0, 1}});
static_assert(rotate_offset({   0, -32 }, {}, rotation::N, rotation::E) == offsets{{ 31,   0}, {1, 0}});
static_assert(rotate_offset({   0, -32 }, {}, rotation::N, rotation::W) == offsets{{-32,   0}, {0, 0}});
static_assert(rotate_offset({ -32,   0 }, {}, rotation::N, rotation::E) == offsets{{  0, -32}, {0, 0}});
static_assert(rotate_offset({ -32,   0 }, {}, rotation::N, rotation::S) == offsets{{ 31,   0}, {1, 0}});
static_assert(rotate_offset({ -32,   0 }, {}, rotation::N, rotation::W) == offsets{{  0,  31}, {0, 1}});
static_assert(rotate_offset({  31,  31 }, {}, rotation::N, rotation::S) == offsets{{-31, -31}, {0, 0}});

// box at (-22, -2) mirrors to (22, 2) and back
static_assert(rotate_offset({ -32,   5 }, { 10, -7 }, rotation::N, rotation::S) == offsets{{ 31, -5}, {-9,  7}});
static_assert(rotate_offset({  31,  -5 }, { -9,  7 }, rotation::S, rotation::N) == offsets{{-31,  5}, { 9, -7}});
// box at (3, -28) turns to (28, 3)
static_assert(rotate_offset({   0, -32 }, {  3,  4 }, rotation::N, rotation::E) == offsets{{ 31,  0}, {-3,  3}});
static_assert(rotate_offset({   0, -32 }, { 0, -127 }, rotation::N, rotation::S) == offsets{{0, 31}, {0, 127}});

} // namespace floormat
