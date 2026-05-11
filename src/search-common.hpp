#pragma once
#include "chunk.hpp"
#include "RTree-search.hpp"
#include "compat/qualified.hpp"
#include <concepts>
#include <mg/Vector2.h>

namespace floormat::Search {

template<std::invocable<object_id, const Chunk_RTree::Rect&> Function, Qualified<chunk> Chunk>
CORRADE_ALWAYS_INLINE
bool is_passable_common(Chunk&& c, Vector2 min, Vector2 max, const Function& p)
{
    auto& rtree = *forward<Chunk>(c).rtree();
    bool ret = !rtree.Search(min.data(), max.data(), p);
    return ret;
}

} // namespace floormat::Search
