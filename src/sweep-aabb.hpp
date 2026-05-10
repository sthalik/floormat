#pragma once
#include "search-pred.hpp"
//#include "pass-mode.hpp"
//#include "object-id.hpp"
#include <mg/Range.h>

namespace Magnum { using Range2D = Math::Range2D<Float>; }
namespace Magnum::Math { template<UnsignedInt dimensions, class T> class Range; }
namespace floormat { class chunk; }

namespace floormat {

struct sweep_result
{
    bool has_collider;
};

sweep_result sweep_aabb_vs_aabb(Range2D start, Vector2 displacement, Range2D obstacle);
sweep_result find_swept_collider(chunk& c, Range2D start, Vector2 displacement, const Search::pred& p);

} // namespace floormat
