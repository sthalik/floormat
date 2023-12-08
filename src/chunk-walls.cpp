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

namespace {

constexpr Vector2 half_tile = TILE_SIZE2*.5f;
constexpr float tile_height = TILE_SIZE.z();

} // namespace

void chunk::ensure_alloc_walls()
{
    if (!_walls) [[unlikely]]
        _walls = Pointer<wall_stuff>{InPlaceInit};
}

// -----------------------

// overlay north
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::overlay, false>(Vector2 center, float depth)
{
    return wall_quad_N(Vector3(center, tile_height), TILE_SIZE);
}

// overlay west
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::overlay, true>(Vector2 center, float depth)
{
    return wall_quad_W(Vector3(center, tile_height), TILE_SIZE);
}

// corner left
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::corner_L, false>(Vector2 center, float depth)
{
    constexpr float x = half_tile.x(), y = half_tile.y(), z = tile_height;
    constexpr float x_offset = (float)(int)x;
    return {{
        {             x + center.x(), -y + center.y(), z + 0 },
        {             x + center.x(), -y + center.y(),     0 },
        { x_offset + -x + center.x(), -y + center.y(), z + 0 },
        { x_offset + -x + center.x(), -y + center.y(),     0 },
    }};
}

// corner right
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::corner_R, true>(Vector2 center, float depth)
{
    constexpr float x = half_tile.x(), y = half_tile.y(), z = tile_height;
    return {{
        {-x + center.x(),  y + center.y(), z + 0 },
        {-x + center.x(),  y + center.y(),     0 },
        {-x + center.x(), -y + center.y(), z + 0 },
        {-x + center.x(), -y + center.y(),     0 },
    }};
}

// wall north
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::wall, false>(Vector2 center, float depth)
{
    return wall_quad_N(Vector3(center, tile_height), TILE_SIZE);
}

// wall west
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::wall, true>(Vector2 center, float depth)
{
    return wall_quad_W(Vector3(center, tile_height), TILE_SIZE);
}

// side north
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::side, false>(Vector2 center, float depth)
{
    constexpr float x = half_tile.x(), y = half_tile.y(), z = tile_height;
    auto left  = Vector2{x + center.x(), -y + center.y()  },
         right = Vector2{left.x(),       left.y() - depth };
    return {{
        { right.x(), right.y(), z + 0 },
        { right.x(), right.y(),     0 },
        { left.x(),  left.y(),  z + 0 },
        { left.x(),  left.y(),      0 },
    }};
}

// side west
template<> std::array<Vector3, 4> chunk::make_wall_vertex_data<Group_::side, true>(Vector2 center, float depth)
{
    constexpr float x = half_tile.x(), y = half_tile.y(), z = tile_height;
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
