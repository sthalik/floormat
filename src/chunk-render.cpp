#include "chunk.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile.hpp"
#include <algorithm>
#include <Magnum/GL/Buffer.h>
#include <Corrade/Containers/ArrayViewStl.h>

namespace floormat {

static auto make_index_array(std::size_t offset)
{
    std::array<std::array<UnsignedShort, 6>, TILE_COUNT> array; // NOLINT(cppcoreguidelines-pro-type-member-init)
    for (std::size_t i = 0; i < TILE_COUNT; i++)
        array[i] = tile_atlas::indices(i + offset);
    return array;
}

struct vertex {
    Vector3 position;
    Vector2 texcoords;
    float depth = -1;
};

auto chunk::ensure_ground_mesh() noexcept -> ground_mesh_tuple
{
    if (!_ground_modified)
        return { ground_mesh, ground_indexes };
    _ground_modified = false;

    for (std::size_t i = 0; i < TILE_COUNT; i++)
        ground_indexes[i] = std::uint8_t(i);
    std::sort(ground_indexes.begin(), ground_indexes.end(), [this](std::uint8_t a, std::uint8_t b) {
        return _ground_atlases[a].get() < _ground_atlases[b].get();
    });

    std::array<std::array<vertex, 4>, TILE_COUNT> vertexes;
    for (std::size_t k = 0; k < TILE_COUNT; k++)
    {
        const std::uint8_t i = ground_indexes[k];
        if (auto atlas = _ground_atlases[i]; !atlas)
            vertexes[k] = {};
        else
        {
            const local_coords pos{i};
            const auto quad = atlas->floor_quad(Vector3(pos.x, pos.y, 0) * TILE_SIZE, TILE_SIZE2);
            const auto texcoords = atlas->texcoords_for_id(_ground_variants[i]);
            const float depth = tile_shader::depth_value(pos);
            auto& v = vertexes[k];
            for (std::size_t j = 0; j < 4; j++)
                v[j] = { quad[j], texcoords[j], depth };
        }
    }
    const auto indexes = make_index_array(0);

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertexes}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{indexes}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(6 * TILE_COUNT);
    ground_mesh = Utility::move(mesh);
    return { ground_mesh, ground_indexes };
}

auto chunk::ensure_wall_mesh() noexcept -> wall_mesh_tuple
{
    if (!_walls_modified)
        return { wall_mesh, wall_indexes };
    _walls_modified = false;

    for (std::size_t i = 0; i < TILE_COUNT*2; i++)
        wall_indexes[i] = std::uint16_t(i);

    std::sort(wall_indexes.begin(), wall_indexes.end(), [this](std::uint16_t a, std::uint16_t b) {
        return _wall_atlases[a] < _wall_atlases[b];
    });

    std::array<std::array<vertex, 4>, TILE_COUNT*2> vertexes;
    for (std::size_t k = 0; k < TILE_COUNT*2; k++)
    {
        const std::uint16_t i = wall_indexes[k];
        if (const auto& atlas = _wall_atlases[i]; !atlas)
            vertexes[k] = {};
        else
        {
            const auto& variant = _wall_variants[i];
            const local_coords pos{i / 2u};
            const auto center = Vector3(pos.x, pos.y, 0) * TILE_SIZE;
            const auto quad = i & 1 ? atlas->wall_quad_W(center, TILE_SIZE) : atlas->wall_quad_N(center, TILE_SIZE);
            const float depth = tile_shader::depth_value(pos);
            const auto texcoords = atlas->texcoords_for_id(variant);
            auto& v = vertexes[k];
            for (std::size_t j = 0; j < 4; j++)
                v[j] = { quad[j], texcoords[j], depth, };
        }
    }

    using index_t = std::array<std::array<UnsignedShort, 6>, TILE_COUNT>;
    const index_t indexes[2] = { make_index_array(0), make_index_array(TILE_COUNT) };

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertexes}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{indexes}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(6 * TILE_COUNT);
    wall_mesh = Utility::move(mesh);
    return { wall_mesh, wall_indexes };
}

} // namespace floormat
