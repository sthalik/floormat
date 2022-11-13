#include "wall.hpp"
#include "tile-atlas.hpp"
#include "shaders/tile.hpp"
#include "chunk.hpp"
#include "tile-image.hpp"
#include "anim-atlas.hpp"
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/MeshView.h>

namespace floormat {

//#define FM_DEBUG_DRAW_COUNT

constexpr auto quad_index_count = 6;

wall_mesh::wall_mesh() = default;

void wall_mesh::draw(tile_shader& shader, chunk& c)
{
    auto [mesh_, ids] = c.ensure_wall_mesh();
    struct { tile_atlas* atlas = nullptr; std::size_t pos = 0; } last;
    GL::MeshView mesh{mesh_};
    [[maybe_unused]] std::size_t draw_count = 0;

    const auto do_draw = [&](std::size_t i, tile_atlas* atlas) {
        if (atlas == last.atlas)
            return;
        if (auto len = i - last.pos; last.atlas && len > 0)
        {
            last.atlas->texture().bind(0);
            mesh.setCount((int)(quad_index_count * len));
            mesh.setIndexRange((int)(last.pos*quad_index_count), 0, quad_index_count*TILE_COUNT*2 - 1);
            shader.draw(mesh);
            draw_count++;
        }
        last = { atlas, i };
    };

    for (std::size_t k = 0; k < TILE_COUNT*2; k++)
        do_draw(k, c.wall_atlas_at(ids[k]));
    do_draw(TILE_COUNT*2, nullptr);

#ifdef FM_DEBUG_DRAW_COUNT
    if (draw_count)
        fm_debug("wall draws: %zu", draw_count);
#endif
}

} // namespace floormat
