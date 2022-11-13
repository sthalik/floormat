#include "floor.hpp"
#include "shaders/tile.hpp"
#include "tile.hpp"
#include "chunk.hpp"
#include "tile-atlas.hpp"
#include <Magnum/GL/MeshView.h>

namespace floormat {

floor_mesh::floor_mesh() = default;

//#define FM_DEBUG_DRAW_COUNT

void floor_mesh::draw(tile_shader& shader, chunk& c)
{
    constexpr auto quad_index_count = 6;

    auto [mesh_, ids] = c.ensure_ground_mesh();

    tile_atlas* last_atlas = nullptr;
    std::size_t last_pos = 0;
    GL::MeshView mesh{mesh_};

    [[maybe_unused]] std::size_t draw_count = 0;

    const auto do_draw = [&](std::size_t i, tile_atlas* atlas) {
        if (atlas == last_atlas)
            return;
        if (auto len = i - last_pos; last_atlas && len > 0)
        {
            last_atlas->texture().bind(0);
            mesh.setCount((int)(quad_index_count * len));
            mesh.setIndexRange((int)(last_pos*quad_index_count), 0, quad_index_count*TILE_COUNT - 1);
            shader.draw(mesh);
            draw_count++;
        }
        last_atlas = atlas;
        last_pos = i;
    };

    for (std::size_t k = 0; k < TILE_COUNT; k++)
        do_draw(k, c.ground_atlas_at(ids[k]));
    do_draw(TILE_COUNT, nullptr);

#ifdef FM_DEBUG_DRAW_COUNT
    if (draw_count)
        fm_debug("floor draws: %zu", draw_count);
#endif
}

} // namespace floormat
