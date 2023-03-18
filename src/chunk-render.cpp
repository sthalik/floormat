#include "chunk.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile.hpp"
#include "entity.hpp"
#include "anim-atlas.hpp"
#include <algorithm>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

template<std::size_t N = 1>
static auto make_index_array(std::size_t max)
{
    std::array<std::array<UnsignedShort, 6>, N*TILE_COUNT> array; // NOLINT(cppcoreguidelines-pro-type-member-init)
    for (auto i = 0_uz; i < max; i++)
        array[i] = tile_atlas::indices(i);
    return array;
}

auto chunk::ensure_ground_mesh() noexcept -> ground_mesh_tuple
{
    if (!_ground_modified)
        return { ground_mesh, ground_indexes, std::size_t(ground_mesh.count()/6) };
    _ground_modified = false;

    std::size_t count = 0;
    for (auto i = 0_uz; i < TILE_COUNT; i++)
        if (_ground_atlases[i])
            ground_indexes[count++] = std::uint8_t(i);

    std::sort(ground_indexes.begin(), ground_indexes.begin() + count,
              [this](std::uint8_t a, std::uint8_t b) {
                  return _ground_atlases[a] < _ground_atlases[b];
              });

    std::array<std::array<vertex, 4>, TILE_COUNT> vertexes;
    for (auto k = 0_uz; k < count; k++)
    {
        const std::uint8_t i = ground_indexes[k];
        const auto& atlas = _ground_atlases[i];
        const local_coords pos{i};
        const auto quad = atlas->floor_quad(Vector3(pos) * TILE_SIZE, TILE_SIZE2);
        const auto texcoords = atlas->texcoords_for_id(_ground_variants[i]);
        const float depth = tile_shader::depth_value(pos);
        auto& v = vertexes[k];
        for (auto j = 0_uz; j < 4; j++)
            v[j] = { quad[j], texcoords[j], depth };
    }

    const auto indexes = make_index_array(count);
    const auto vertex_view = ArrayView{vertexes.data(), count};
    const auto vert_index_view = ArrayView{indexes.data(), count};

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{vert_index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(std::int32_t(6 * count));
    ground_mesh = Utility::move(mesh);
    return { ground_mesh, ground_indexes, count };
}

auto chunk::ensure_wall_mesh() noexcept -> wall_mesh_tuple
{
    if (!_walls_modified)
        return { wall_mesh, wall_indexes, std::size_t(wall_mesh.count()/6) };
    _walls_modified = false;

    std::size_t count = 0;
    for (auto i = 0_uz; i < TILE_COUNT*2; i++)
        if (_wall_atlases[i])
            wall_indexes[count++] = std::uint16_t(i);

    std::sort(wall_indexes.begin(), wall_indexes.begin() + count,
              [this](std::uint16_t a, std::uint16_t b) {
                  return _wall_atlases[a] < _wall_atlases[b];
              });

    std::array<std::array<vertex, 4>, TILE_COUNT*2> vertexes;
    for (auto k = 0_uz; k < count; k++)
    {
        const std::uint16_t i = wall_indexes[k];
        const auto& atlas = _wall_atlases[i];
        const auto& variant = _wall_variants[i];
        const local_coords pos{i / 2u};
        const auto center = Vector3(pos) * TILE_SIZE;
        const auto quad = i & 1 ? atlas->wall_quad_W(center, TILE_SIZE) : atlas->wall_quad_N(center, TILE_SIZE);
        const float depth = tile_shader::depth_value(pos);
        const auto texcoords = atlas->texcoords_for_id(variant);
        auto& v = vertexes[k];
        for (auto j = 0_uz; j < 4; j++)
            v[j] = { quad[j], texcoords[j], depth, };
    }

    auto indexes = make_index_array<2>(count);
    const auto vertex_view = ArrayView{vertexes.data(), count};
    const auto vert_index_view = ArrayView{indexes.data(), count};

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{vert_index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(std::int32_t(6 * count));
    wall_mesh = Utility::move(mesh);
    return { wall_mesh, wall_indexes, count };
}

auto chunk::ensure_scenery_mesh() noexcept -> scenery_mesh_tuple
{
    fm_assert(_entities_sorted);

    if (_scenery_modified)
    {
        _scenery_modified = false;

        const auto count = fm_begin(
            std::size_t ret = 0;
            for (const auto& e : _entities)
                ret += !e->is_dynamic();
            return ret;
        );

        scenery_indexes.clear();
        scenery_indexes.reserve(count);
        scenery_vertexes.clear();
        scenery_vertexes.reserve(count);

        for (const auto& e : _entities)
        {
            if (e->atlas->info().fps > 0)
                continue;

            const auto i = scenery_indexes.size();
            scenery_indexes.emplace_back();
            scenery_indexes.back() = tile_atlas::indices(i);
            const auto& atlas = e->atlas;
            const auto& fr = *e;
            const auto pos = e->coord.local();
            const auto coord = Vector3(pos) * TILE_SIZE + Vector3(Vector2(fr.offset), 0);
            const auto quad = atlas->frame_quad(coord, fr.r, fr.frame);
            const auto& group = atlas->group(fr.r);
            const auto texcoords = atlas->texcoords_for_frame(fr.r, fr.frame, !group.mirror_from.isEmpty());
            const float depth = tile_shader::depth_value(pos, tile_shader::scenery_depth_offset);
            scenery_vertexes.emplace_back();
            auto& v = scenery_vertexes.back();
            for (auto j = 0_uz; j < 4; j++)
                v[j] = { quad[j], texcoords[j], depth };
        }

        GL::Mesh mesh{GL::MeshPrimitive::Triangles};
        mesh.addVertexBuffer(GL::Buffer{scenery_vertexes}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
            .setIndexBuffer(GL::Buffer{scenery_indexes}, 0, GL::MeshIndexType::UnsignedShort)
            .setCount(std::int32_t(6 * count));
        scenery_mesh = Utility::move(mesh);
    }
    return { scenery_mesh, };
}

} // namespace floormat
