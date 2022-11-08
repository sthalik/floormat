#pragma once

#include "local-coords.hpp"
#include "scenery.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace floormat::Serialize { struct anim_frame; }

namespace floormat {

struct tile_shader;
struct anim_atlas;
using anim_frame = Serialize::anim_frame;

struct anim_mesh final
{
    anim_mesh();
    void draw(tile_shader& shader, const anim_atlas& atlas, rotation r, std::size_t frame, local_coords xy);

private:
    struct vertex_data final { Vector2 texcoords; };
    using quad_data = std::array<vertex_data, 4>;

    static std::array<UnsignedShort, 6> make_index_array();

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{quad_data{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{make_index_array()}, _positions_buffer{std::array<Vector3, 4>{}};
};

} // namespace floormat
