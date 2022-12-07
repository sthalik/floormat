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
    const auto [mesh_, ids, size] = c.ensure_wall_mesh();
    struct { tile_atlas* atlas = nullptr; std::size_t pos = 0; } last;
    GL::MeshView mesh{mesh_};
    [[maybe_unused]] std::size_t draw_count = 0;

    const auto do_draw = [&](std::size_t i, tile_atlas* atlas, std::uint32_t max_index) {
        if (atlas == last.atlas)
            return;
        if (auto len = i - last.pos; last.atlas && len > 0)
        {
            last.atlas->texture().bind(0);
            mesh.setCount((int)(quad_index_count * len));
            mesh.setIndexRange((int)(last.pos*quad_index_count), 0, max_index);
            shader.draw(mesh);
            draw_count++;
        }
        last = { atlas, i };
    };

    const auto max_index = std::uint32_t(size*quad_index_count - 1);
    std::size_t k;
    for (k = 0; k < size; k++)
        do_draw(k, c.wall_atlas_at(ids[k]), max_index);
    do_draw(size, nullptr, max_index);

#ifdef FM_DEBUG_DRAW_COUNT
    if (draw_count)
        fm_debug("wall draws: %zu", draw_count);
#endif
}

} // namespace floormat
