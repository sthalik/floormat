#include "chunk.hpp"
#include "object-storage.inl"
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
#include "sprite-atlas.hpp"

namespace floormat {

void chunk::add_clickables(const tile_shader& shader, Vector2i win_size, Array<clickable>& array, bool draw_vobjs)
{
    for (const auto& obj : _objects)
        if (draw_vobjs || !obj->is_virtual())
            SpriteBatch::add_clickable(*obj, shader, win_size, array);
}

void chunk::ensure_scenery_mesh(SpriteBatch& sb, bool render_vobjs)
{
    fm_assert(_objects._sorted);

    const bool modify_static = _scenery_modified;
    _scenery_modified = false;
    if (modify_static)
    {
        scenery_static_mesh.clear();
        // upper bound: six quads per static object, none for a dynamic one
        scenery_static_mesh.reserve(6 * (uint32_t)_objects.size());
    }

    sb.begin_chunk((uint32_t)_objects.size()); // static objects go to scenery_static_mesh instead

    for (const auto& eʹ : _objects)
    {
        auto& e = *eʹ;
        const bool is_dynamic = e.is_dynamic();
        if (!is_dynamic && !modify_static)
            continue;
        if (is_dynamic && !render_vobjs && e.is_virtual())
            continue;

        const auto& atlas = e.atlas;
        //const auto pos = e->coord.local();
        //const auto coord = Vector3(pos) * TILE_SIZE + Vector3(Vector2(fr.offset), 0);
        const auto pt = e.position();
        const auto center = pt + Vector2i(e.bbox_offset);
        const auto quad = atlas->frame_quad(Vector3(pt), e.r, e.frame);
        const auto& group = atlas->group(e.r);
        const auto* sp = group.sprites[e.frame];
        fm_assert(sp);
        const auto uv3 = loader.atlas().texcoords_for(sprite{sp}, !group.mirror_from.isEmpty());
        const auto& frame = atlas->frame(e.r, e.frame);
        const float depth_start = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;
        const auto depth_offset = e.depth_offset();

        if (is_dynamic)
        {
            const auto depth = Depth::value_at(depth_start, center, depth_offset);
            const auto v = Quads::make_vertexes(quad, uv3, depth);
            sb.emit(v, depth);
        }
        else
        {
            // --- slope-based sprite split ---
            // the box spans [center - lo, center + hi)
            const auto lo = Vector2i(e.bbox_size/2), hi = Vector2i(e.bbox_size) - lo;
            const auto corner = [&](Vector2i px) {
                return tile_shader::project(Vector3(Vector2(Vector2i(e.bbox_offset) + px), 0.f) - Vector3(group.offset));
            };
            const auto nw = corner(-lo), se = corner(hi), w = corner({-lo.x(), hi.y()}), east = corner({hi.x(), -lo.y()});
            const float slope = east.x() > w.x() ? (east.y() - w.y()) / (east.x() - w.x()) : 0.f;

            // sprite screen extent (pixel offsets from projected center)
            const float left_x   = float(-frame.ground.x());
            const float right_x  = float(frame.size.x()) - float(frame.ground.x());
            const float sprite_h = float(frame.size.y());
            const float bottom_y = float(frame.size.y()) - float(frame.ground.y());

            //const auto depth_bias = int32_t((uint32_t)e.bbox_size.min());
            const auto depth_bias = hi.sum();
            const auto front_depth      = Depth::value_at(depth_start, center, depth_offset + depth_bias);
            const auto back_left_depth  = Depth::value_at(depth_start, center, depth_offset + hi.y() - lo.x());
            const auto back_right_depth = Depth::value_at(depth_start, center, depth_offset + hi.x() - lo.y());

            const float x_nw = Math::clamp(nw.x(), left_x, right_x);
            const float x_se = Math::clamp(se.x(), left_x, right_x);
            const float xs[4] = { left_x, Math::min(x_nw, x_se), Math::max(x_nw, x_se), right_x };

            // t: 0 = sprite bottom, 1 = top
            auto t_at = [&](float x) {
                const float y = w.y() + slope * (x - w.x());
                return Math::clamp((bottom_y - y) / sprite_h, 0.f, 1.f);
            };
            // depth of the bbox's south and east sides at this screen column, parallel to
            // north and west wall faces so thin walls in front hide the whole sprite. One screen
            // pixel along either side is one unit of depth.
            auto depth_at = [&](float x) {
                const float xc = Math::clamp(x, w.x(), east.x());
                const float south = (float)(hi.y() - lo.x()) + (xc - w.x()),
                            east_side = (float)(hi.x() - lo.y()) + (east.x() - xc);
                const auto off = (int32_t)Math::floor(Math::min(south, east_side));
                return Depth::value_at(depth_start, center, depth_offset + off);
            };
            // quad[0]=BR, quad[1]=TR, quad[2]=BL, quad[3]=TL
            auto vert = [&](float x, float t, float depth) -> Quads::vertex {
                const float s = (x - left_x) / (right_x - left_x);
                return { quad[2] + s * (quad[0] - quad[2]) + t * (quad[3] - quad[2]),
                         uv3[2] + s * (uv3[0] - uv3[2]) + t * (uv3[3] - uv3[2]),
                         depth };
            };

            for (uint32_t i = 0; i < 3; i++)
            {
                const float xa = xs[i], xb = xs[i+1];
                if (!(xb > xa))
                    continue;
                const float ta = t_at(xa), tb = t_at(xb);
                const float da = depth_at(xa), db = depth_at(xb);

                if (ta > 0.f || tb > 0.f)
                {
                    const Quads::vertexes v = {{
                        vert(xb, 0.f, db), vert(xb, tb, db),
                        vert(xa, 0.f, da), vert(xa, ta, da),
                    }};
                    scenery_static_mesh.add(v, front_depth, &e);
                }
                if (ta < 1.f || tb < 1.f)
                {
                    const float back_depth = xa + xb < 2 * x_nw ? back_left_depth : back_right_depth;
                    const Quads::vertexes v = {{
                        vert(xb, tb, db), vert(xb, 1.f, db),
                        vert(xa, ta, da), vert(xa, 1.f, da),
                    }};
                    scenery_static_mesh.add(v, back_depth, &e);
                }
            }
            // --- end slope-based split ---
        }
    }
    sb.end_chunk<true>();

    if (modify_static)
        sb.sort_by_depth(scenery_static_mesh);
    sb.emit(scenery_static_mesh, render_vobjs);
}

} // namespace floormat
