#include "chunk.hpp"
#include "src/tile-bbox.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/PairStl.h>

// +x +y +z
// +x +y -z
// -x -y +z
// -x +y -z

namespace floormat {

using Wall::Group_;

// wall north
template<> auto chunk::make_wall_vertex_data<Group_::wall, false>(size_t tile, float depth) -> vertex
{
}



} // namespace floormat
