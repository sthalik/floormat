#include "app.hpp"
#include "src/chunk.hpp"
#include "src/collision.hpp"
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector4.h>

namespace floormat {

void test_app::test_bbox()
{
    auto c = make_test_chunk();

    constexpr auto pos1 = sTILE_SIZE2 * (TILE_MAX_DIM/2) - Vector2s(0, sTILE_SIZE2[1]/2);
    constexpr auto b1 = Vector4s{pos1[0], pos1[1], sTILE_SIZE2[0], sTILE_SIZE2[1]};
    constexpr auto pos2 = Vector2s(iTILE_SIZE2 * (Vector2i(TILE_MAX_DIM/2) - Vector2i(-1, -1)) - Vector2i(0, iTILE_SIZE[1]/2));
    constexpr auto b2 = Vector4s{pos2[0], pos2[1], usTILE_SIZE2[0], usTILE_SIZE2[1]};
    {
        auto q1 = c.query_collisions(b1, collision::move);
        fm_assert(q1);
        auto q2 = c.query_collisions(b2, collision::move);
        fm_assert(!q2);
    }
}

} // namespace floormat
