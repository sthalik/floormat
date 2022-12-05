#include "app.hpp"
#include "src/chunk.hpp"
#include "compat/LooseQuadtree-impl.h"
#include <Magnum/Math/Vector2.h>

namespace floormat {

void test_app::test_bbox()
{
    auto c = make_test_chunk();
    auto& qt = c.ensure_passability();
    fm_assert(qt.GetSize() >= 2);

    using namespace loose_quadtree;
    using bbox = BoundingBox<std::int16_t>;
    constexpr auto pos1 = Vector2s(iTILE_SIZE2 * (TILE_MAX_DIM/2) - Vector2i(0, iTILE_SIZE[1]/2)),
                   size = Vector2s(iTILE_SIZE2);
    constexpr auto b1 = bbox{pos1[0], pos1[1], size[0], size[1]};
    constexpr auto pos2 = Vector2s(iTILE_SIZE2 * (Vector2i(TILE_MAX_DIM/2) - Vector2i(-1, -1)) - Vector2i(0, iTILE_SIZE[1]/2));
    auto q1 = qt.QueryIntersectsRegion(b1);
    fm_assert(!q1.EndOfQuery());
    do q1.Next(); while (!q1.EndOfQuery());
    constexpr auto b2 = bbox{pos2[0], pos2[1], size[0], size[1]};
    auto q2 = qt.QueryIntersectsRegion(b2);
    fm_assert(q2.EndOfQuery());
}

} // namespace floormat
