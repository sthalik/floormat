#include "chunk.hpp"
#include "tile-constants.hpp"
#include "ground-atlas.hpp"
#include "quads.hpp"
#include "shaders/shader.hpp"
#include <algorithm>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

using namespace floormat::Quads;

namespace {

static Array<std::array<chunk::vertex, 4>> static_vertexes{NoInit, TILE_COUNT};

} // namespace

template<size_t N>
std::array<std::array<UnsignedShort, 6>, N*TILE_COUNT>
chunk::make_index_array(size_t max)
{
    std::array<std::array<UnsignedShort, 6>, N*TILE_COUNT> array; // NOLINT(cppcoreguidelines-pro-type-member-init)
    for (auto i = 0uz; i < max; i++)
        array[i] = quad_indexes(i);
    return array;
}

template std::array<std::array<UnsignedShort, 6>, 1*TILE_COUNT> chunk::make_index_array<1>(size_t);
template std::array<std::array<UnsignedShort, 6>, 2*TILE_COUNT> chunk::make_index_array<2>(size_t);

void chunk::ensure_alloc_ground()
{
    if (!_ground) [[unlikely]]
        _ground = Pointer<ground_stuff>{InPlaceInit};
}

auto chunk::ensure_ground_mesh() noexcept -> ground_mesh_tuple
{
    if (!_ground)
        return { ground_mesh, {}, 0 };

    if (!_ground_modified)
        return { ground_mesh, _ground->indexes, size_t(ground_mesh.count()/6) };
    _ground_modified = false;

    size_t count = 0;
    for (auto i = 0uz; i < TILE_COUNT; i++)
        if (_ground->atlases[i])
            _ground->indexes[count++] = uint8_t(i);

    std::sort(_ground->indexes.begin(), _ground->indexes.begin() + count,
              [this](uint8_t a, uint8_t b) {
                  return _ground->atlases[a] < _ground->atlases[b];
              });

    float hack_offset = _coord.z <= 0 ? -16.f : 0.f; // XXX hack
    auto& vertexes = static_vertexes;

    for (auto k = 0uz; k < count; k++)
    {
        const uint8_t i = _ground->indexes[k];
        const auto& atlas = _ground->atlases[i];
        const local_coords pos{i};
        const auto quad = floor_quad(Vector3(pos) * TILE_SIZE, TILE_SIZE2);
        const auto texcoords = atlas->texcoords_for_id(_ground->variants[i] % _ground->atlases[i]->num_tiles());
        const float depth = tile_shader::depth_value(pos, tile_shader::ground_depth_offset + hack_offset);
        auto& v = vertexes[k];
        for (auto j = 0uz; j < 4; j++)
            v[j] = { quad[j], texcoords[j], depth };
    }

    const auto indexes = make_index_array<1>(count);
    const auto vertex_view = ArrayView{vertexes.data(), count};
    const auto vert_index_view = ArrayView{indexes.data(), count};

    GL::Mesh mesh{GL::MeshPrimitive::Triangles};
    mesh.addVertexBuffer(GL::Buffer{vertex_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(GL::Buffer{vert_index_view}, 0, GL::MeshIndexType::UnsignedShort)
        .setCount(int32_t(6 * count));
    ground_mesh = move(mesh);
    return { ground_mesh, _ground->indexes, count };
}


} // namespace floormat
