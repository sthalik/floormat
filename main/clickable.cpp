#include "clickable.hpp"
#include "src/object.hpp"
#include "src/anim-atlas.hpp"
#include "src/point.inl"
#include "src/depth.hpp"
#include "shaders/shader.hpp"

namespace floormat {

bool clickable_hit_test(const clickable& c, Vector2i pixel)
{
    if (!c.dest.contains(pixel))
        return false;
    if (c.bitmask.isEmpty())
        return true;
    // mirror inside the frame, before adding the frame's atlas offset
    const auto posʹ = pixel - c.dest.min();
    const auto posʹʹ = !c.mirrored ? posʹ : Vector2i(int(c.src.sizeX()) - 1 - posʹ[0], posʹ[1]);
    const auto pos = posʹʹ + Vector2i(c.src.min());
    const usize idx = unsigned(pos.y()) * c.stride + unsigned(pos.x());
    fm_assert(idx < c.bitmask.size());
    return c.bitmask[idx];
}

Optional<clickable> clickable_for_object(const object& e, Vector2i win_size, Vector2i camera)
{
    const auto& a = *e.atlas;
    const auto& g = a.group(e.r);
    const auto& f = a.frame(e.r, e.frame);
    const auto offset = tile_shader::point_to_pixelʹ(Vector3i(e.position()) + Vector3i(g.offset), win_size, camera)
                        - f.ground;
    if (!(offset < win_size && offset + Vector2i(f.size) >= Vector2i()).all())
        return {};
    return clickable {
        .src = {f.offset, f.offset + f.size},
        .dest = {offset, offset + Vector2i(f.size)},
        .bitmask = a.bitmask(),
        .id = e.id,
        .depth = Depth::value_atʹ(e.position()),
        .stride = a.info().pixel_size[0],
        .mirrored = !g.mirror_from.isEmpty(),
    };
}

} // namespace floormat
