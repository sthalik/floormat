#pragma once

#include "local-coords.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>

namespace floormat::Serialize { struct anim_atlas; struct anim_frame; }

namespace floormat {

struct anim_atlas;
using anim_frame = Serialize::anim_frame;

struct anim_mesh final
{
    anim_mesh();
    void draw(local_coords pos, const anim_atlas& atlas, const anim_frame& frame);

private:
    struct vertex_data final { Vector2 texcoords; };
    using quad_data = std::array<vertex_data, 4>;

    static std::array<UnsignedShort, 6> make_index_array();


    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{quad_data{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{make_index_array()}, _positions_buffer{quad_data{}};
};

} // namespace floormat
