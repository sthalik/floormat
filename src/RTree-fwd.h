#pragma once
#include "object-id.hpp"

template<class DATATYPE, class ELEMTYPE, int NUMDIMS,
         class ELEMTYPEREAL = ELEMTYPE, int TMAXNODES = 8, int TMINNODES = TMAXNODES / 2>
class old_rtree;

namespace floormat {

using Chunk_RTree = ::old_rtree<object_id, float, 2, float>;

} // namespace floormat
