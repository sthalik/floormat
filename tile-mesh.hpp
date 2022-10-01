#pragma once
#include "tile-atlas.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace Magnum::Examples {

struct tile_shader;
struct tile_image;

class tile_mesh final
{
    static constexpr std::size_t index_count = std::tuple_size_v<decltype(tile_atlas{}.indices(0))>;
    static constexpr std::array<UnsignedShort, index_count> _indices =
        tile_atlas::indices(0);

    struct vertex_data final {
        Vector3 position;
        Vector2 texcoords;
    };
    std::array<vertex_data, 4> _vertex_data = {};
    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{_vertex_data, Magnum::GL::BufferUsage::DynamicDraw},
        _index_buffer{_indices, Magnum::GL::BufferUsage::StaticDraw};

public:
    tile_mesh();
    tile_mesh(tile_mesh&&) = delete;
    tile_mesh(const tile_mesh&) = delete;

    void draw_quad(tile_shader& shader, tile_image& img, const std::array<Vector3, 4>& positions);
    void draw_floor_quad(tile_shader& shader, tile_image& img, Vector3 center);
};

} // namespace Magnum::Examples
