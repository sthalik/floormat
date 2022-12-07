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
    constexpr int quad_index_count = 6;
    const auto [mesh_, ids, size] = c.ensure_ground_mesh();
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
        do_draw(k, c.ground_atlas_at(ids[k]), max_index);
    do_draw(size, nullptr, max_index);

#ifdef FM_DEBUG_DRAW_COUNT
    if (draw_count)
        fm_debug("floor draws: %zu", draw_count);
#endif
}

} // namespace floormat
