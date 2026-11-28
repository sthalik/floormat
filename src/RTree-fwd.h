#pragma once
#include "object-id.hpp"

template<class DATATYPE, class ELEMTYPE, int NUMDIMS,
         class ELEMTYPEREAL = ELEMTYPE, int TMAXNODES = 8, int TMINNODES = TMAXNODES / 2>
class old_rtree;

namespace floormat {

template<typename ELEMTYPE, u32 max_entries_before_tree> struct spatial_index;

using Chunk_RTree = spatial_index<i16, 128>;

} // namespace floormat
