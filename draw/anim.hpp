#pragma once
#include "src/local-coords.hpp"
#include "src/rotation.hpp"
#include "src/chunk.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include "main/clickable.hpp"

//namespace floormat::Serialize { struct anim_frame; }

namespace floormat {

struct tile_shader;
class anim_atlas;
class chunk;
struct clickable;
struct object;

struct anim_mesh
{
    anim_mesh();

    void draw(tile_shader& shader, const Vector2i& win_size, chunk& c, Array<clickable>& list, bool draw_vobjs);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& pos, float depth);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, local_coords xy, Vector2b offset, float dpeth);
    static void add_clickable(tile_shader& shader, const Vector2i& win_size,
                              object* s, const chunk::topo_sort_data& data,
                              Array<clickable>& list);

private:
    struct vertex_data
    {
        Vector3 position;
        Vector2 texcoords;
        float depth = -1;
    };
    using quad_data = std::array<vertex_data, 4>;

    Array<chunk::object_draw_order> _draw_array;
    Array<std::array<uint16_t, 6>> _draw_indexes;
    Array<std::array<chunk::vertex, 4>> _draw_vertexes;

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer, _index_buffer;
};

} // namespace floormat
