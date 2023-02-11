#pragma once
#include "local-coords.hpp"
#include <array>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Buffer.h>
#include "src/scenery.hpp"
#include "main/clickable.hpp"

//namespace floormat::Serialize { struct anim_frame; }

namespace floormat {

struct tile_shader;
struct anim_atlas;
struct chunk;
template<typename Atlas, typename T> struct clickable;
struct scenery;

struct anim_mesh final
{
    using clickable_scenery = clickable<anim_atlas, scenery>;

    anim_mesh();

    void draw(tile_shader& shader, chunk& c);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, std::size_t frame, const Vector3& pos, float depth);
    void draw(tile_shader& shader, anim_atlas& atlas, rotation r, std::size_t frame, local_coords xy, Vector2b offset);
    static void add_clickable(tile_shader& shader, const Vector2i& win_size,
                                  chunk_coords c, std::uint8_t i, const std::shared_ptr<anim_atlas>& atlas, scenery& s,
                                  std::vector<clickable_scenery>& clickable);

private:
    struct vertex_data final {
        Vector3 position;
        Vector2 texcoords;
        float depth = -1;
    };
    using quad_data = std::array<vertex_data, 4>;

    static std::array<UnsignedShort, 6> make_index_array();

    GL::Mesh _mesh;
    GL::Buffer _vertex_buffer{quad_data{}, Magnum::GL::BufferUsage::DynamicDraw},
               _index_buffer{make_index_array()};
};

} // namespace floormat
