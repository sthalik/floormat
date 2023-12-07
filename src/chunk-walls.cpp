#include "chunk.hpp"
#include "tile-bbox.hpp"
#include "quads.hpp"
#include "wall-atlas.hpp"
#include "shaders/shader.hpp"
#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/PairStl.h>
#include <algorithm>

// +x +y +z
// +x +y -z
// -x -y +z
// -x +y -z

namespace floormat {

using namespace floormat::Quads;
using Wall::Group_;


void chunk::ensure_alloc_walls()
{
    if (!_walls) [[unlikely]]
        _walls = Pointer<wall_stuff>{InPlaceInit};
}

// -----------------------

// wall north
template<> auto chunk::make_wall_vertex_data<Group_::wall, false>(size_t tile, float depth) -> vertex
{
}

// -----------------------

fm_noinline
GL::Mesh chunk::make_wall_mesh(size_t count)
{
    fm_debug_assert(_walls);
    //std::array<std::array<vertex, 4>, TILE_COUNT*2> vertexes;
    vertex vertexes[TILE_COUNT*2][4];
    for (auto k = 0uz; k < count; k++)
    {
        const uint16_t i = _walls->indexes[k];
        const auto& atlas = _walls->atlases[i];
        const auto& variant = _walls->variants[i];
        const local_coords pos{i / 2u};
        const auto center = Vector3(pos) * TILE_SIZE;
        const auto quad = i & 1 ? wall_quad_W(center, TILE_SIZE) : wall_quad_N(center, TILE_SIZE);
        const float depth = tile_shader::depth_value(pos, tile_shader::wall_depth_offset);
        const auto texcoords = atlas->texcoords_for_id(variant);
        auto& v = vertexes[k];
        for (auto j = 0uz; j < 4; j++)
            v[j] = { quad[j], texcoords[j], depth, };
    }

    auto indexes = make_index_array<2>(count);
    const auto vertex_view = ArrayView{&vertexes[0], count};
    const auto vert_index_view = ArrayView{indexes.data(), count};

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{vert_index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * count));
    return mesh;
}

auto chunk::ensure_wall_mesh() noexcept -> wall_mesh_tuple
{
    if (!_walls)
        return {wall_mesh, {}, 0};

    if (!_walls_modified)
        return { wall_mesh, _walls->wall_indexes, size_t(wall_mesh.count()/6) };
    _walls_modified = false;

    size_t count = 0;
    for (auto i = 0uz; i < TILE_COUNT*2; i++)
        if (_walls->_wall_atlases[i])
            _walls->wall_indexes[count++] = uint16_t(i);

    std::sort(_walls->wall_indexes.data(), _walls->wall_indexes.data() + (ptrdiff_t)count,
              [this](uint16_t a, uint16_t b) {
                  return _walls->_wall_atlases[a] < _walls->_wall_atlases[b];
              });

    wall_mesh = make_wall_mesh(count);
    return { wall_mesh, _walls->wall_indexes, count };
}

} // namespace floormat
