#include "chunk.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile.hpp"
#include <algorithm>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty)
        return false;

    for (std::size_t i = 0; i < TILE_COUNT; i++)
    {
        if (_ground_atlases[i] || _wall_north_atlases[i] || _wall_west_atlases[i])
        {
            _maybe_empty = false;
            return false;
        }
    }

    return true;
}

tile_atlas* chunk::ground_atlas_at(std::size_t i) const noexcept
{
    return _ground_atlases[i].get();
}

static constexpr auto make_index_array()
{
    std::array<std::array<UnsignedShort, 6>, TILE_COUNT> array;
    for (std::size_t i = 0; i < TILE_COUNT; i++)
        array[i] = tile_atlas::indices(i);
    return array;
}

auto chunk::ensure_ground_mesh() noexcept -> mesh_tuple
{
    if (!_ground_modified)
        return { ground_mesh, ground_indexes };
    _ground_modified = false;

    for (std::size_t i = 0; i < TILE_COUNT; i++)
        ground_indexes[i] = std::uint8_t(i);
    std::sort(ground_indexes.begin(), ground_indexes.end(), [this](std::uint8_t a, std::uint8_t b) {
        return _ground_atlases[a].get() < _ground_atlases[b].get();
    });

    struct vertex {
        Vector3 position;
        Vector2 texcoords;
        float depth = -1;
    };
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
            const auto texcoords = atlas->texcoords_for_id(_ground_variants[i] % atlas->num_tiles());
            const float depth = tile_shader::depth_value(pos);
            auto& v = vertexes[k];
            for (std::size_t j = 0; j < 4; j++)
                v[j] = { quad[j], texcoords[j], depth };
        }
    }
    constexpr auto indexes = make_index_array();

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertexes}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{indexes}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(6 * TILE_COUNT);
    ground_mesh = Utility::move(mesh);
    return { ground_mesh, ground_indexes };
}

chunk::chunk() noexcept // NOLINT(modernize-use-equals-default)
{
    //fm_debug("chunk ctor");
}

tile_ref chunk::operator[](std::size_t idx) noexcept { return { *this, std::uint8_t(idx) }; }
tile_proto chunk::operator[](std::size_t idx) const noexcept { return tile_proto(tile_ref { *const_cast<chunk*>(this), std::uint8_t(idx) }); }
tile_ref chunk::operator[](local_coords xy) noexcept { return operator[](xy.to_index()); }
tile_proto chunk::operator[](local_coords xy) const noexcept { return operator[](xy.to_index()); }

auto chunk::begin() noexcept -> iterator { return iterator { *this, 0 }; }
auto chunk::end() noexcept -> iterator { return iterator { *this, TILE_COUNT }; }
auto chunk::cbegin() const noexcept -> const_iterator { return const_iterator { *this, 0 }; }
auto chunk::cend() const noexcept -> const_iterator { return const_iterator { *this, TILE_COUNT }; }
auto chunk::begin() const noexcept -> const_iterator { return cbegin(); }
auto chunk::end() const noexcept -> const_iterator { return cend(); }

chunk::chunk(chunk&&) noexcept = default;
chunk& chunk::operator=(chunk&&) noexcept = default;

void chunk::mark_modified() noexcept
{
    _ground_modified = true;
}

bool chunk::is_modified() const noexcept
{
    return _ground_modified;
}

} // namespace floormat
