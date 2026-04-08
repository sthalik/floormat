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
    //scenery_dynamic_mesh.clear();

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
        const auto texcoords = atlas->texcoords_for_frame(e.r, e.frame, !group.mirror_from.isEmpty());
        const auto& frame = atlas->frame(e.r, e.frame);
        const float depth_start = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;
        const auto depth_offset = e.depth_offset();
        constexpr auto f = tile_shader::foreshortening_factor;

        if (is_dynamic)
        {
            const auto depth = Depth::value_at(depth_start, pt, depth_offset);
            Quads::vertexes v;
            for (uint8_t j = 0; j < 4; j++)
                v[j] = {quad[j], texcoords[j], depth};
            sb.emit(atlas->texture(), v, depth);
            //scenery_dynamic_mesh.add(v, &atlas->texture(), depth);
        }
        else
        {
            // --- slope-based sprite split ---
            const auto bb_half = Vector2(e.bbox_size) * 0.5f;
            const float denom = bb_half.x() + bb_half.y();
            const float slope = denom > 0.f ? f * (bb_half.x() - bb_half.y()) / denom : 0.f;

            // bbox center screen offset from projected object center
            const auto bbox_scr = tile_shader::project(Vector3(Vector2(e.bbox_offset), 0.f));

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
            const auto right_split_uv  = texcoords[0] + t_right * (texcoords[1] - texcoords[0]);
            const auto right_split_pos = quad[0] + t_right * (quad[1] - quad[0]);
            const auto left_split_uv   = texcoords[2] + t_left * (texcoords[3] - texcoords[2]);
            const auto left_split_pos  = quad[2] + t_left * (quad[3] - quad[2]);

            //const auto depth_bias = int32_t((uint32_t)e.bbox_size.min());
            const auto depth_bias = int32_t((Vector2ui(e.bbox_size)/2).sum());
            const auto front_depth = Depth::value_at(depth_start, pt, depth_offset + depth_bias);
            const auto back_depth = Depth::value_at(depth_start, pt, depth_offset);

#if 0
            const auto vec = Vector3i(pt);
            const auto center = Vector2i(vec.x(), vec.y()) + Vector2i(e.bbox_offset);
            const auto hx = (Int)bb_half.x(), hy = (Int)bb_half.y();
            const auto h_3 = Vector2i{hx, hy} / 3;
            // diagonal from NW corner (-hx, -hy) to SE corner (+hx, +hy)
            // front triangle (SE half): average of (hx,hy), (hx,-hy), (-hx,hy)
            const auto front_pt = point(Vector3i(center + h_3, _coord.z));
            // back triangle (NW half): average of (-hx,-hy), (hx,-hy), (-hx,hy)
            const auto back_pt = point(Vector3i(center - h_3, _coord.z));
            const auto line_length = (int32_t)(2.f * Math::sqrt((float)(hx*hx + hy*hy) + 0.5f));
            const auto front_depth = Depth::value_at(depth_start, back_pt, depth_offset); // flipped - not a typo
            const auto back_depth  = Depth::value_at(depth_start, front_pt, depth_offset);
#endif

            Quads::vertexes v1, v2;

            // front quad (below slope line, closer to camera)
            v1[0] = {quad[0], texcoords[0], front_depth}; // BR
            v1[1] = {right_split_pos, right_split_uv, front_depth}; // right split
            v1[2] = {quad[2], texcoords[2], front_depth}; // BL
            v1[3] = {left_split_pos, left_split_uv, front_depth}; // left split
            scenery_static_mesh.add(v1, &atlas->texture(), front_depth, &e);

            // back quad (above slope line, further from camera)
            v2[0] = {right_split_pos, right_split_uv, back_depth}; // right split
            v2[1] = {quad[1], texcoords[1], back_depth}; // TR
            v2[2] = {left_split_pos, left_split_uv, back_depth}; // left split
            v2[3] = {quad[3], texcoords[3], back_depth}; // TL
            scenery_static_mesh.add(v2, &atlas->texture(), back_depth, &e);
            // --- end slope split ---

            //i += 2;
        }
    }
    sb.end_chunk(true);

    constexpr auto less = [](const auto& a, const auto& b) {
        const auto& [av, at, ad, ao] = a;
        const auto& [bv, bt, bd, bo] = b;
        return ad < bd;
    };
    if (modify_static)
        ranges::sort(ranges::zip_view(scenery_static_mesh.Vertexes,
                                      scenery_static_mesh.Textures,
                                      scenery_static_mesh.Depths,
                                      scenery_static_mesh.Objects),
                     less);
    sb.emit(scenery_static_mesh, render_vobjs);
}

} // namespace floormat
