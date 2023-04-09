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

    void draw(tile_shader& shader, const Vector2i& win_size, chunk& c, std::vector<clickable>& list);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& pos, float depth);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, local_coords xy, Vector2b offset, float dpeth);
    static void add_clickable(tile_shader& shader, const Vector2i& win_size,
                              entity* s_, const chunk::topo_sort_data& data,
                              std::vector<clickable>& list);

private:
    static constexpr size_t batch_size = 256, quad_index_count = 6;
    static std::array<UnsignedShort, quad_index_count> make_index_array();
    static std::array<std::array<UnsignedShort, quad_index_count>, batch_size> make_batch_index_array();

    struct vertex_data final {
        Vector3 position;
        Vector2 texcoords;
        float depth = -1;
    };
    using quad_data = std::array<vertex_data, 4>;

    Array<chunk::entity_draw_order> _draw_array;
    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{quad_data{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{make_index_array()};

    GL::Mesh _batch_mesh{NoCreate};
    GL::Buffer _batch_vertex_buffer{std::array<quad_data, batch_size>{}, Magnum::GL::BufferUsage::DynamicDraw},
               _batch_index_buffer{make_batch_index_array()};
};

} // namespace floormat
