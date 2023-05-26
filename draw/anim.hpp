#pragma once
#include "local-coords.hpp"
#include "rotation.hpp"
#include "chunk.hpp"
#include <array>
#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include "main/clickable.hpp"

//namespace floormat::Serialize { struct anim_frame; }

namespace floormat {

struct tile_shader;
struct anim_atlas;
struct chunk;
struct clickable;
struct entity;

struct anim_mesh final
{
    anim_mesh();

    void draw(tile_shader& shader, const Vector2i& win_size, chunk& c, std::vector<clickable>& list, bool draw_vobjs);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& pos, float depth);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, local_coords xy, Vector2b offset, float dpeth);
    static void add_clickable(tile_shader& shader, const Vector2i& win_size,
                              entity* s_, const chunk::topo_sort_data& data,
                              std::vector<clickable>& list);

private:
    static std::array<UnsignedShort, 6> make_index_array();

    struct vertex_data final {
        Vector3 position;
        Vector2 texcoords;
        float depth = -1;
    };
    using quad_data = std::array<vertex_data, 4>;

    Array<chunk::entity_draw_order> _draw_array;
    Array<std::array<uint16_t, 6>> _draw_indexes;
    Array<std::array<chunk::vertex, 4>> _draw_vertexes;

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{quad_data{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{make_index_array()};
};

} // namespace floormat
