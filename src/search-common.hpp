#pragma once
#include "chunk.hpp"
#include "RTree-search.hpp"
#include <mg/Vector2.h>

namespace floormat::Search {
template<typename Function>
CORRADE_ALWAYS_INLINE
bool is_passable_common(chunk& c, Vector2 min, Vector2 max, const Function& p)
{
    auto& rtree = *c.rtree();
    bool ret = !rtree.Search(min.data(), max.data(), p);
    return ret;
}

} // namespace floormat::Search
