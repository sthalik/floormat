#pragma once
#include "defs.hpp"
#include "tile.hpp"
#include "tile-atlas.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace Magnum::Examples {

struct tile_shader;
struct chunk;

struct floor_mesh final
{
    floor_mesh();
    floor_mesh(floor_mesh&&) = delete;
    floor_mesh(const floor_mesh&) = delete;

    void set_tile(tile& x, local_coords pt);
    void draw(tile_shader& shader, chunk& c);

private:
    struct vertex_data final {
        Vector2 texcoords;
    };
    using quad_data = std::array<vertex_data, 4>;
    using vertex_positions_data = std::array<Vector3, 4>;
    static constexpr auto quad_index_count = std::tuple_size_v<decltype(tile_atlas::indices(0))>;
    using index_type = std::array<UnsignedShort, quad_index_count>;

    static const std::array<index_type, TILE_COUNT> _index_data;
    static const std::array<vertex_positions_data, TILE_COUNT> _position_data;

    GL::Mesh _mesh;
    std::array<quad_data, TILE_COUNT> _vertex_data = {};

    GL::Buffer _vertex_buffer{_vertex_data, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{_index_data, Magnum::GL::BufferUsage::StaticDraw},
               _positions_buffer{_position_data, Magnum::GL::BufferUsage::StaticDraw};
};

} // namespace Magnum::Examples
