#include "chunk-scenery.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"
#include "object.hpp"
#include "anim-atlas.hpp"
#include "scenery.hpp"
#include "quads.hpp"
#include "depth.hpp"
#include "renderer.hpp"
#include <bit>
#include <algorithm>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

auto chunk::ensure_scenery_mesh() noexcept -> scenery_mesh_tuple
{
    Array<object_draw_order> array;
    Array<Quads::vertexes> scenery_vertexes;
    Array<Quads::indexes> scenery_indexes;
    return ensure_scenery_mesh({array, scenery_vertexes, scenery_indexes});
}

auto chunk::ensure_scenery_mesh(scenery_scratch_buffers buffers) noexcept -> scenery_mesh_tuple
{
    ensure_scenery_buffers(buffers);

    fm_assert(_objects_sorted);

    const float depth_start = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;

    if (_scenery_modified)
    {
        _scenery_modified = false;

        const auto count = [this] {
            uint32_t ret = 0;
            for (const auto& e : _objects)
                ret += !e->is_dynamic();
            return ret;
        }();

        auto& scenery_vertexes = buffers.scenery_vertexes;
        auto& scenery_indexes = buffers.scenery_indexes;

        for (auto i = 0u; const auto& e : _objects)
        {
            if (e->is_dynamic())
                continue;
            const auto& atlas = e->atlas;
            fm_debug_assert(atlas != nullptr);
            const auto& fr = *e;
            const auto pos = e->coord.local();
            const auto coord = Vector3(pos) * TILE_SIZE + Vector3(Vector2(fr.offset), 0);
            const auto quad = atlas->frame_quad(coord, fr.r, fr.frame);
            const auto& group = atlas->group(fr.r);
            const auto texcoords = atlas->texcoords_for_frame(fr.r, fr.frame, !group.mirror_from.isEmpty());
            const float back_depth  = Depth::value_at(depth_start, e->position(), e->depth_offset());
            const float front_depth = Depth::value_at(depth_start, e->position(), e->depth_offset() + 2);

            // --- slope-based sprite split ---
            const auto& frame = atlas->frame(fr.r, fr.frame);
            const auto bb_half = Vector2(e->bbox_size) * 0.5f;
            const float denom = bb_half.x() + bb_half.y();
            const float slope = denom > 0.f
                ? tile_shader::foreshortening_factor * (bb_half.x() - bb_half.y()) / denom
                : 0.f;

            // bbox center screen offset from projected object center
            const auto bbox_scr = tile_shader::project(Vector3(Vector2(e->bbox_offset), 0.f));

            // sprite screen extent (pixel offsets from projected center)
            const float left_x   = float(-frame.ground.x());
            const float right_x  = float(frame.size.x()) - float(frame.ground.x());
            const float sprite_h = float(frame.size.y());
            const float bottom_y = float(frame.size.y()) - float(frame.ground.y());

            // slope line y-value at left and right sprite edges
            const float y_at_left  = bbox_scr.y() + slope * (left_x  - bbox_scr.x());
            const float y_at_right = bbox_scr.y() + slope * (right_x - bbox_scr.x());

            // t-values on left/right edges: 0 = bottom, 1 = top
            const float t_left  = Math::clamp((bottom_y - y_at_left)  / sprite_h, 0.f, 1.f);
            const float t_right = Math::clamp((bottom_y - y_at_right) / sprite_h, 0.f, 1.f);

            // split points on left edge (BL→TL) and right edge (BR→TR)
            // quad[0]=BR, quad[1]=TR, quad[2]=BL, quad[3]=TL
            const auto right_split_uv  = texcoords[0] + t_right * (texcoords[1] - texcoords[0]);
            const auto right_split_pos = quad[0]      + t_right * (quad[1]      - quad[0]);
            const auto left_split_uv   = texcoords[2] + t_left  * (texcoords[3] - texcoords[2]);
            const auto left_split_pos  = quad[2]      + t_left  * (quad[3]      - quad[2]);

            // front quad (below slope line, closer to camera)
            scenery_vertexes[i][0] = { quad[0],          texcoords[0],    front_depth };  // BR
            scenery_vertexes[i][1] = { right_split_pos,  right_split_uv,  front_depth };  // right split
            scenery_vertexes[i][2] = { quad[2],          texcoords[2],    front_depth };  // BL
            scenery_vertexes[i][3] = { left_split_pos,   left_split_uv,   front_depth };  // left split
            scenery_indexes[i] = Quads::quad_indexes(i);

            // back quad (above slope line, further from camera)
            scenery_vertexes[i+1][0] = { right_split_pos,  right_split_uv,  back_depth };  // right split
            scenery_vertexes[i+1][1] = { quad[1],          texcoords[1],    back_depth };  // TR
            scenery_vertexes[i+1][2] = { left_split_pos,   left_split_uv,   back_depth };  // left split
            scenery_vertexes[i+1][3] = { quad[3],          texcoords[3],    back_depth };  // TL
            scenery_indexes[i+1] = Quads::quad_indexes(i+1);
            // --- end slope split ---

            i += 2;
        }

        const auto quad_count = count * 2;
        if (quad_count == 0)
            scenery_mesh = GL::Mesh{NoCreate};
        else
        {
            GL::Mesh mesh{GL::MeshPrimitive::Triangles};
            auto vert_view = ArrayView<const Quads::vertexes>{scenery_vertexes, quad_count};
            auto index_view = ArrayView<const Quads::indexes>{scenery_indexes, quad_count};
            mesh.addVertexBuffer(GL::Buffer{vert_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
                .setIndexBuffer(GL::Buffer{index_view}, 0, GL::MeshIndexType::UnsignedShort)
                .setCount(int32_t(6 * quad_count));
            scenery_mesh = move(mesh);
        }
    }

    const auto size = _objects.size();
    auto& array = buffers.array;
    uint32_t j = 0, i = 0;
    for (const auto& e : _objects)
    {
        auto index = e->is_dynamic() ? (uint32_t)-1 : j;
        array[i++] = { e.get(), index };
        if (!e->is_dynamic()) j += 2;
    }
    std::sort(array.data(), array.data() + i,
              [d=depth_start](const object_draw_order& a, const object_draw_order& b) {
                  return Depth::value_at(d, a.e->position()) < Depth::value_at(d, b.e->position());
              });

    return { scenery_mesh, ArrayView<object_draw_order>{array, size}, j };
}

void chunk::ensure_scenery_buffers(scenery_scratch_buffers bufs)
{
    const size_t lenʹ = _objects.size() * 2;

    if (lenʹ <= bufs.array.size())
        return;

    size_t len;

    if (lenʹ > 1 << 20)
        len = lenʹ;
    else
        len = std::bit_ceil(lenʹ);

    bufs.array = Array<object_draw_order>{NoInit, len};
    bufs.scenery_vertexes = Array<Quads::vertexes>{NoInit, len};
    bufs.scenery_indexes = Array<Quads::indexes>{NoInit, len};
}

} // namespace floormat
