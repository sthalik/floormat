#include "app.hpp"
#include "src/chunk.hpp"
#include "src/collision.hpp"
#include <Magnum/Math/Vector2.h>

namespace floormat {

static void test_simple(chunk c)
{
    auto& qt = c.ensure_passability();
    fm_assert(qt.GetSize() >= 2);

    using namespace loose_quadtree;
    using bbox = BoundingBox<std::int16_t>;
    constexpr auto pos1 = sTILE_SIZE2 * (TILE_MAX_DIM/2) - Vector2s(0, sTILE_SIZE2[1]/2);
    constexpr auto b1 = bbox{pos1[0], pos1[1], usTILE_SIZE2[0], usTILE_SIZE2[1]};
    auto q1 = qt.QueryIntersectsRegion(b1);
    fm_assert(!q1.EndOfQuery());
    auto x1 = *q1.GetCurrent();
    fm_assert(x1.pass_mode == pass_mode::blocked);
    do q1.Next(); while (!q1.EndOfQuery());
    constexpr auto pos2 = Vector2s(iTILE_SIZE2 * (Vector2i(TILE_MAX_DIM/2) - Vector2i(-1, -1)) - Vector2i(0, iTILE_SIZE[1]/2));
    constexpr auto b2 = bbox{pos2[0], pos2[1], usTILE_SIZE2[0], usTILE_SIZE2[1]};
    auto q2 = qt.QueryIntersectsRegion(b2);
    fm_assert(q2.EndOfQuery());
}

static void test_wrapper(chunk c)
{
    {
        int i = 0;
        for (auto b : c.query_collisions(local_coords{TILE_MAX_DIM/2, TILE_MAX_DIM/2},
                                         usTILE_SIZE2,
                                         Vector2s(sTILE_SIZE2[0]/2, 0)))
        {
            fm_assert(b.pass_mode == pass_mode::blocked);
            i++;
        }
        fm_assert(i > 0);
    }
}

void test_app::test_bbox()
{
    test_simple(make_test_chunk());
    test_wrapper(make_test_chunk());
}

} // namespace floormat
