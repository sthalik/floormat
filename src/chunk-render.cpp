#include "chunk.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile.hpp"
#include <algorithm>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

template<size_t N = 1>
static auto make_index_array(size_t max)
{
    std::array<std::array<UnsignedShort, 6>, N*TILE_COUNT> array; // NOLINT(cppcoreguidelines-pro-type-member-init)
    for (auto i = 0uz; i < max; i++)
        array[i] = tile_atlas::indices(i);
    return array;
}

auto chunk::ensure_ground_mesh() noexcept -> ground_mesh_tuple
{
    if (!_ground_modified)
        return { ground_mesh, ground_indexes, size_t(ground_mesh.count()/6) };
    _ground_modified = false;

    size_t count = 0;
    for (auto i = 0uz; i < TILE_COUNT; i++)
        if (_ground_atlases[i])
            ground_indexes[count++] = uint8_t(i);

    std::sort(ground_indexes.begin(), ground_indexes.begin() + count,
              [this](uint8_t a, uint8_t b) {
                  return _ground_atlases[a] < _ground_atlases[b];
              });

    std::array<std::array<vertex, 4>, TILE_COUNT> vertexes;
    for (auto k = 0uz; k < count; k++)
    {
        const uint8_t i = ground_indexes[k];
        const auto& atlas = _ground_atlases[i];
        const local_coords pos{i};
        const auto quad = atlas->floor_quad(Vector3(pos) * TILE_SIZE, TILE_SIZE2);
        const auto texcoords = atlas->texcoords_for_id(_ground_variants[i]);
        const float depth = tile_shader::depth_value(pos);
        auto& v = vertexes[k];
        for (auto j = 0uz; j < 4; j++)
            v[j] = { quad[j], texcoords[j], depth };
    }

    const auto indexes = make_index_array(count);
    const auto vertex_view = ArrayView{vertexes.data(), count};
    const auto vert_index_view = ArrayView{indexes.data(), count};

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{vert_index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * count));
    ground_mesh = Utility::move(mesh);
    return { ground_mesh, ground_indexes, count };
}

fm_noinline
GL::Mesh chunk::make_wall_mesh(size_t count)
{
    //std::array<std::array<vertex, 4>, TILE_COUNT*2> vertexes;
    vertex vertexes[TILE_COUNT*2][4];
    for (auto k = 0uz; k < count; k++)
    {
        const uint16_t i = wall_indexes[k];
        const auto& atlas = _wall_atlases[i];
        const auto& variant = _wall_variants[i];
        const local_coords pos{i / 2u};
        const auto center = Vector3(pos) * TILE_SIZE;
        const auto quad = i & 1 ? atlas->wall_quad_W(center, TILE_SIZE) : atlas->wall_quad_N(center, TILE_SIZE);
        const float depth = tile_shader::depth_value(pos);
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
    if (!_walls_modified)
        return { wall_mesh, wall_indexes, size_t(wall_mesh.count()/6) };
    _walls_modified = false;

    size_t count = 0;
    for (auto i = 0uz; i < TILE_COUNT*2; i++)
        if (_wall_atlases[i])
            wall_indexes[count++] = uint16_t(i);

    std::sort(wall_indexes.data(), wall_indexes.data() + (ptrdiff_t)count,
              [this](uint16_t a, uint16_t b) {
                  return _wall_atlases[a] < _wall_atlases[b];
              });

    wall_mesh = make_wall_mesh(count);
    return { wall_mesh, wall_indexes, count };
}

} // namespace floormat
