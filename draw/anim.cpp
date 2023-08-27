#include "anim.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "shaders/shader.hpp"
#include "main/clickable.hpp"
#include "chunk-scenery.hpp"
#include <cstdio>
#include <Corrade/Containers/Optional.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

anim_mesh::anim_mesh()
{
    _mesh.setCount(6)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

std::array<UnsignedShort, 6> anim_mesh::make_index_array()
{
    return {{
        0, 1, 2,
        2, 1, 3,
    }};
}

void anim_mesh::add_clickable(tile_shader& shader, const Vector2i& win_size,
                              entity* s_, const chunk::topo_sort_data& data,
                              std::vector<clickable>& list)
{
    const auto& s = *s_;
    const auto& a = *s.atlas;
    const auto& g = a.group(s.r);
    const auto& f = a.frame(s.r, s.frame);
    const auto world_pos = TILE_SIZE20 * Vector3(s.coord.local()) + Vector3(g.offset) + Vector3(Vector2(s.offset), 0);
    const Vector2i offset((Vector2(shader.camera_offset()) + Vector2(win_size)*.5f)
                          + shader.project(world_pos) - Vector2(f.ground));
    if (offset < win_size && offset + Vector2i(f.size) >= Vector2i())
    {
        clickable item = {
            .src = { f.offset, f.offset + f.size },
            .dest = { offset, offset + Vector2i(f.size) },
            .bitmask = a.bitmask(),
            .e = s_,
            .depth = s.ordinal() + (float)s.coord.z() * TILE_COUNT,
            .slope = data.slope,
            .bb_min = data.bb_min, .bb_max = data.bb_max,
            .stride = a.info().pixel_size[0],
            .mirrored = !g.mirror_from.isEmpty(),
        };
        list.push_back(item);
    }
}

void anim_mesh::draw(tile_shader& shader, const Vector2i& win_size, chunk& c, std::vector<clickable>& list, bool draw_vobjs)
{
    constexpr auto quad_index_count = 6;

    auto [mesh_, es, size] = c.ensure_scenery_mesh({ _draw_array, _draw_vertexes, _draw_indexes });
    GL::MeshView mesh{mesh_};
    const auto max_index = uint32_t(size*quad_index_count - 1);

    uint32_t i = 0;

    for (const auto& x : es)
    {
        fm_assert(x.e);
        add_clickable(shader, win_size, x.data.in, x.data, list);
        auto& e = *x.e;

        auto& atlas = *e.atlas;
        fm_assert(e.is_dynamic() == (x.mesh_idx == (uint32_t)-1));
        if (!e.is_dynamic())
        {
            fm_assert(i < size);
            GL::MeshView mesh{mesh_};
            mesh.setCount((int)(quad_index_count * 1));
            mesh.setIndexRange((int)(x.mesh_idx*quad_index_count), 0, max_index);
            shader.draw(atlas.texture(), mesh);
            i++;
        }
        else
        {
            if (!draw_vobjs) [[likely]]
                if (e.is_virtual()) [[unlikely]]
                    continue;

            const auto depth0 = e.depth_offset();
            const auto depth = tile_shader::depth_value(e.coord.local(), depth0);
            draw(shader, atlas, e.r, e.frame, e.coord.local(), e.offset, depth);
        }
    }
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& center, float depth)
{
    const auto pos = atlas.frame_quad(center, r, frame);
    const auto& g = atlas.group(r);
    const auto texcoords = atlas.texcoords_for_frame(r, frame, !g.mirror_from.isEmpty());
    quad_data array;
    for (auto i = 0uz; i < 4; i++)
        array[i] = { pos[i], texcoords[i], depth };
    _vertex_buffer.setSubData(0, array);
    shader.draw(atlas.texture(), _mesh);
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, local_coords xy, Vector2b offset, float depth)
{
    const auto pos = Vector3(xy) * TILE_SIZE + Vector3(Vector2(offset), 0);
    draw(shader, atlas, r, frame, pos, depth);
}


} // namespace floormat
