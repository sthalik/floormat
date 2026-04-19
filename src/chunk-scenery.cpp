#include "chunk.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"
#include "object.hpp"
#include "anim-atlas.hpp"
#include "quads.hpp"
#include "point.inl"
#include "depth.hpp"
#include "renderer.hpp"
#include "spritebatch.hpp"
#include "loader/loader.hpp"
#include "loader/sprite-atlas.hpp"
#include <algorithm>
#include <ranges>

namespace floormat {
namespace ranges = std::ranges;

void chunk::add_clickables(const tile_shader& shader, Vector2i win_size, Array<clickable>& array, bool draw_vobjs)
{
    for (const auto& obj : _objects)
        if (draw_vobjs || !obj->is_virtual())
            SpriteBatch::add_clickable(&*obj, shader, win_size, array);
}

void chunk::ensure_scenery_mesh(SpriteBatch& sb, bool render_vobjs)
{
    fm_assert(_objects_sorted);

    const bool modify_static = _scenery_modified;
    _scenery_modified = false;
    if (modify_static)
        scenery_static_mesh.clear();

    sb.begin_chunk();

    for (const auto& eʹ : _objects)
    {
        auto& e = *eʹ;
        const bool is_dynamic = e.is_dynamic();
        if (!is_dynamic && !modify_static)
            continue;

        const auto& atlas = e.atlas;
        //const auto pos = e->coord.local();
        //const auto coord = Vector3(pos) * TILE_SIZE + Vector3(Vector2(fr.offset), 0);
        const auto pt = e.position();
        const auto quad = atlas->frame_quad(Vector3(pt), e.r, e.frame);
        const auto& group = atlas->group(e.r);
        const auto* sp = group.sprites[e.frame];
        fm_assert(sp);
        const auto uv3 = loader.atlas().texcoords_for(sprite{sp}, !group.mirror_from.isEmpty());
        const auto& frame = atlas->frame(e.r, e.frame);
        const float depth_start = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;
        const auto depth_offset = e.depth_offset();
        constexpr auto f = tile_shader::foreshortening_factor;

        if (is_dynamic)
        {
            const auto depth = Depth::value_at(depth_start, pt, depth_offset);
            Quads::vertexes v;
            for (uint8_t j = 0; j < 4; j++)
                v[j] = {quad[j], uv3[j], depth};
            if (!render_vobjs && e.is_virtual())
                continue;
            sb.emit(v, uv3, depth);
        }
        else
        {
            // --- slope-based sprite split ---
            const auto bb_half = Vector2(e.bbox_size) * 0.5f;
            const float denom = bb_half.x() + bb_half.y();
            const float slope = denom > 0.f ? f * (bb_half.x() - bb_half.y()) / denom : 0.f;

            // bbox center screen offset from sprite's ground anchor
            const auto bbox_scr = tile_shader::project(Vector3(Vector2(e.bbox_offset), 0.f) - Vector3(group.offset));

            // sprite screen extent (pixel offsets from projected center)
            const float left_x   = float(-frame.ground.x());
            const float right_x  = float(frame.size.x()) - float(frame.ground.x());
            const float sprite_h = float(frame.size.y());
            const float bottom_y = float(frame.size.y()) - float(frame.ground.y());

            // slope line y-value at left and right sprite edges
            const float y_at_left  = bbox_scr.y() + slope * (left_x - bbox_scr.x());
            const float y_at_right = bbox_scr.y() + slope * (right_x - bbox_scr.x());

            // t-values on left/right edges: 0 = bottom, 1 = top
            const float t_left = Math::clamp((bottom_y - y_at_left) / sprite_h, 0.f, 1.f);
            const float t_right = Math::clamp((bottom_y - y_at_right) / sprite_h, 0.f, 1.f);

            // split points on left edge (BL→TL) and right edge (BR→TR)
            // quad[0]=BR, quad[1]=TR, quad[2]=BL, quad[3]=TL
            const auto right_split_uv  = uv3[0] + t_right * (uv3[1] - uv3[0]);
            const auto right_split_pos = quad[0] + t_right * (quad[1] - quad[0]);
            const auto left_split_uv   = uv3[2] + t_left * (uv3[3] - uv3[2]);
            const auto left_split_pos  = quad[2] + t_left * (quad[3] - quad[2]);

            //const auto depth_bias = int32_t((uint32_t)e.bbox_size.min());
            const auto depth_bias = int32_t((Vector2ui(e.bbox_size)/2).sum());
            const auto front_depth      = Depth::value_at(depth_start, pt, depth_offset + depth_bias);
            const auto back_left_depth  = Depth::value_at(depth_start, pt, depth_offset + int(bb_half.y()) - int(bb_half.x()));
            const auto back_right_depth = Depth::value_at(depth_start, pt, depth_offset + int(bb_half.x()) - int(bb_half.y()));

            // front quad (below slope line, closer to camera)
            Quads::vertexes v1 = {{
                {quad[0], uv3[0], front_depth},                   // BR
                {right_split_pos, right_split_uv, front_depth},   // right split
                {quad[2], uv3[2], front_depth},                   // BL
                {left_split_pos, left_split_uv, front_depth},     // left split
            }};
            Quads::texcoords u1 = {{ uv3[0], right_split_uv, uv3[2], left_split_uv }};
            scenery_static_mesh.add(v1, u1, front_depth, &e);

            // midpoints for vertical split of back quad
            const auto center_split_pos = (left_split_pos + right_split_pos) * 0.5f;
            const auto center_split_uv  = (left_split_uv + right_split_uv) * 0.5f;
            const auto center_top_pos   = (quad[3] + quad[1]) * 0.5f;
            const auto center_top_uv    = (uv3[3] + uv3[1]) * 0.5f;

            // back-left quad (above slope, screen-left half)
            Quads::vertexes v2 = {{
                {center_split_pos, center_split_uv, back_left_depth},  // BR
                {center_top_pos, center_top_uv, back_left_depth},      // TR
                {left_split_pos, left_split_uv, back_left_depth},      // BL
                {quad[3], uv3[3], back_left_depth},                    // TL
            }};
            Quads::texcoords u2 = {{ center_split_uv, center_top_uv, left_split_uv, uv3[3] }};
            scenery_static_mesh.add(v2, u2, back_left_depth, &e);

            // back-right quad (above slope, screen-right half)
            Quads::vertexes v3 = {{
                {right_split_pos, right_split_uv, back_right_depth},   // BR
                {quad[1], uv3[1], back_right_depth},                   // TR
                {center_split_pos, center_split_uv, back_right_depth}, // BL
                {center_top_pos, center_top_uv, back_right_depth},     // TL
            }};
            Quads::texcoords u3 = {{ right_split_uv, uv3[1], center_split_uv, center_top_uv }};
            scenery_static_mesh.add(v3, u3, back_right_depth, &e);
            // --- end 3-piece split ---

            //i += 2;
        }
    }
    sb.end_chunk(true);

    constexpr auto less = [](const auto& a, const auto& b) {
        const auto& [av, au, ad, ao] = a;
        const auto& [bv, bu, bd, bo] = b;
        return ad < bd;
    };
    if (modify_static)
        ranges::sort(ranges::zip_view(scenery_static_mesh.Vertexes,
                                      scenery_static_mesh.UVs,
                                      scenery_static_mesh.Depths,
                                      scenery_static_mesh.Objects),
                     less);
    sb.emit(scenery_static_mesh, render_vobjs);
}

} // namespace floormat
