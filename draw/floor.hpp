#pragma once
#include "tile-defs.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

struct tile_shader;
struct chunk;
struct tile;

struct floor_mesh final
{
    floor_mesh();
    floor_mesh(floor_mesh&&) = delete;
    floor_mesh(const floor_mesh&) = delete;

    void draw(tile_shader& shader, chunk& c);

private:
    struct vertex_data final { Vector2 texcoords; };
    using quad_data = std::array<vertex_data, 4>;

    static std::array<std::array<UnsignedShort, 6>, TILE_COUNT> make_index_array();
    static std::array<std::array<Vector3, 4>, TILE_COUNT> make_position_array();

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{std::array<quad_data, TILE_COUNT>{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{make_index_array()}, _positions_buffer{make_position_array()};

    static void set_tile(quad_data& data, tile& x);
};

} // namespace floormat
