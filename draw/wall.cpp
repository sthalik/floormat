#include "wall.hpp"
#include "src/wall-atlas.hpp"
#include "shaders/shader.hpp"
#include "src/chunk.hpp"
//#include "src/tile-image.hpp"
//#include "src/anim-atlas.hpp"
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/MeshView.h>

namespace floormat {

constexpr auto quad_index_count = 6;

wall_mesh::wall_mesh() = default;

void wall_mesh::draw(tile_shader& shader, chunk& c)
{
    const auto [mesh_, ids, size] = c.ensure_wall_mesh();
    struct { wall_atlas* atlas = nullptr; size_t pos = 0; } last;
    GL::MeshView mesh{mesh_};
    [[maybe_unused]] size_t draw_count = 0;
    fm_debug_assert(size_t(mesh_.count()) == size*quad_index_count);

    const auto do_draw = [&](size_t i, wall_atlas* atlas, uint32_t max_index) {
        if (atlas == last.atlas)
            return;
        if (auto len = i - last.pos; last.atlas && len > 0)
        {
            mesh.setCount((int)(quad_index_count * len));
            mesh.setIndexOffset((int)(last.pos*quad_index_count), 0, max_index);
            shader.draw(last.atlas->texture(), mesh);
            draw_count++;
        }
        last = { atlas, i };
    };

    const auto max_index = uint32_t(size*quad_index_count - 1);
    size_t k;
    for (k = 0; k < size; k++)
        do_draw(k, c.wall_atlas_at(ids[k]), max_index);
    if (size)
        do_draw(size, nullptr, max_index);

//#define FM_DEBUG_DRAW_COUNT
#ifdef FM_DEBUG_DRAW_COUNT
    if (draw_count)
        fm_debug("wall draws: %zu", draw_count);
#endif
}

} // namespace floormat
