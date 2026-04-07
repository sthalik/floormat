#include "anim.hpp"
#include "src/tile-constants.hpp"
#include "src/anim-atlas.hpp"
#include "src/chunk.hpp"
#include "shaders/shader.hpp"
#include "main/clickable.hpp"
#include "src/chunk-scenery.hpp"
#include "src/scenery.hpp"
#include "src/depth.hpp"
#include "src/renderer.hpp"
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

namespace {

Quads::indexes make_index_array()
{
    return {{
        0, 1, 2,
        2, 1, 3,
    }};
}

struct minmax_s { uint32_t len, min; };

} // namespace

anim_mesh::anim_mesh() :
    _vertex_buffer{quad_data{}, Magnum::GL::BufferUsage::DynamicDraw},
    _index_buffer{make_index_array()}
{
    _mesh.setCount(6)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

void anim_mesh::add_clickable(tile_shader& shader, const Vector2i& win_size, object* sʹ, Array<clickable>& list)
{
    const auto& s = *sʹ;
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
            .e = sʹ,
            .stride = a.info().pixel_size[0],
            .mirrored = !g.mirror_from.isEmpty(),
        };
        arrayAppend(list, item);
    }
}

void anim_mesh::draw(tile_shader& shader, const Vector2i& win_size, chunk& c, Array<clickable>& list, bool draw_vobjs)
{
    constexpr auto quad_index_count = 6;

    auto [mesh_, es, size] = c.ensure_scenery_mesh({ _draw_array, _draw_vertexes, _draw_indexes, });
    const auto max_index = uint32_t(size*quad_index_count - 1);
    const float depth_start = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;

    uint32_t k = 0;

    while (k < es.size())
    {
        const auto& x = es[k];
        fm_assert(x.e);
        add_clickable(shader, win_size, x.e, list);
        auto& e = *x.e;

        auto& atlas = *e.atlas;
        fm_assert(e.is_dynamic() == (x.mesh_idx == (uint32_t)-1));
        if (!e.is_dynamic())
        {
            GL::MeshView mesh{mesh_};
            mesh.setCount(quad_index_count * 2);
            mesh.setIndexOffset((int)(x.mesh_idx*quad_index_count), 0, max_index);
            shader.draw(atlas.texture(), mesh);
            k++;
        }
        else
        {
            const auto& frame = atlas.frame(e.r, e.frame);
            const auto center_pt = e.position();
            const int left_offset = -frame.ground.x();
            const int right_offset = int(frame.size.x()) - frame.ground.x();
            const auto left_pt  = center_pt + Vector2i{left_offset, 0};
            const auto right_pt = center_pt + Vector2i{right_offset, 0};
            auto left_depth  = Depth::value_at(depth_start, left_pt,  e.depth_offset());
            auto right_depth = Depth::value_at(depth_start, right_pt, e.depth_offset());
            k++;
            if (e.is_virtual()) [[unlikely]]
            {
                left_depth = 1;
                right_depth = 1;
                if (!draw_vobjs) [[likely]]
                    continue;
            }
            const Quads::depths depth = {right_depth, right_depth, left_depth, left_depth};
            draw(shader, atlas, e.r, e.frame, e.coord.local(), e.offset, depth);
        }
    }
    fm_assert(k == es.size());
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& center, const Quads::depths& depth)
{
    const auto pos = atlas.frame_quad(center, r, frame);
    const auto& g = atlas.group(r);
    const auto texcoords = atlas.texcoords_for_frame(r, frame, !g.mirror_from.isEmpty());
    quad_data array;
    for (auto i = 0uz; i < 4; i++)
        array[i] = { pos[i], texcoords[i], depth[i] };
    _vertex_buffer.setSubData(0, array);
    shader.draw(atlas.texture(), _mesh);
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, local_coords xy, Vector2b offset, const Quads::depths& depth)
{
    const auto pos = Vector3(xy) * TILE_SIZE + Vector3(Vector2(offset), 0);
    draw(shader, atlas, r, frame, pos, depth);
}

} // namespace floormat
