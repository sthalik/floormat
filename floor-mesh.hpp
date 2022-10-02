#pragma once
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

    void draw(tile_shader& shader, chunk& c);

private:
    static constexpr auto quad_index_count = 6;

    struct vertex_data final {
        Vector2 texcoords;
    };
    using quad_data = std::array<vertex_data, 4>;
    using quad_positions_data = std::array<Vector3, 4>;
    using index_type = std::array<UnsignedShort, quad_index_count>;

    static void set_tile(quad_data& data, tile& x);

    static const std::array<index_type, TILE_COUNT> _index_data;
    static const std::array<quad_positions_data, TILE_COUNT> _position_data;

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{_index_data, Magnum::GL::BufferUsage::StaticDraw},
               _positions_buffer{_position_data, Magnum::GL::BufferUsage::StaticDraw};
};

} // namespace Magnum::Examples
