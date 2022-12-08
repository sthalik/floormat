#include "anim.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "shaders/tile.hpp"
#include "main/clickable.hpp"
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Texture.h>

//#define FM_DEBUG_DRAW_COUNT

namespace floormat {

anim_mesh::anim_mesh()
{
    _mesh.setCount(6)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

std::array<UnsignedShort, 6> anim_mesh::make_index_array()
{
    return {{
        0, 1, 2,
        2, 1, 3,
    }};
}

void anim_mesh::add_clickable(tile_shader& shader, const Vector2i& win_size,
                              chunk_coords c, std::uint8_t i, const std::shared_ptr<anim_atlas>& atlas, scenery& s,
                              std::vector<clickable_scenery>& clickable)
{
    const local_coords xy{i};
    const auto& g = atlas->group(s.r);
    const auto& f = atlas->frame(s.r, s.frame);
    const auto world_pos = TILE_SIZE20 * Vector3(xy.x, xy.y, 0) + Vector3(g.offset);
    const Vector2ui offset((Vector2(shader.camera_offset()) + Vector2(win_size)*.5f)
                           + shader.project(world_pos) - Vector2(f.ground));
    clickable_scenery item = {
        *atlas, s,
        { f.offset, f.offset + f.size }, { offset, offset + f.size },
        atlas->bitmask(), tile_shader::depth_value(xy, tile_shader::scenery_depth_offset), c, xy,
        !g.mirror_from.isEmpty(),
    };
    clickable.push_back(item);
}

void anim_mesh::draw(tile_shader& shader, chunk& c)
{
    constexpr auto quad_index_count = 6;
    auto [mesh_, ids, size] = c.ensure_scenery_mesh();
    struct { anim_atlas* atlas = nullptr; std::size_t pos = 0; } last;
    GL::MeshView mesh{mesh_};
    anim_atlas* bound = nullptr;

    [[maybe_unused]] std::size_t draw_count = 0;

    const auto do_draw = [&](std::size_t i, anim_atlas* atlas, std::uint32_t max_index, bool force) {
        if (!force && atlas == last.atlas)
            return;
        if (auto len = i - last.pos; last.atlas && len > 0)
        {
            if (last.atlas != bound)
                last.atlas->texture().bind(0);
            bound = last.atlas;
            mesh.setCount((int)(quad_index_count * len));
            mesh.setIndexRange((int)(last.pos*quad_index_count), 0, max_index);
            shader.draw(mesh);
            draw_count++;
        }
        last = { atlas, i };
    };

    constexpr auto next_is_dynamic = [](chunk& c, std::size_t i) {
        for (; i < TILE_COUNT; i++)
            if (auto [atlas, s] = c[i].scenery(); atlas && atlas->info().fps == 0)
                return false;
        return true;
    };

    const auto draw_dynamic = [&](std::size_t last_id, std::size_t id) {
        for (std::size_t i = last_id+1; i < id; i++)
            if (auto [atlas, s] = c[i].scenery(); atlas && atlas->info().fps > 0)
            {
                draw(shader, *atlas, s.r, s.frame, local_coords{i});
                bound = nullptr;
            }
    };

    const auto max_index = std::uint32_t(size*quad_index_count - 1);
    std::size_t k, last_id = 0;
    for (k = 0; k < size; k++)
    {
        const auto id = ids[k];
        draw_dynamic(last_id, id);
        do_draw(k, c.scenery_atlas_at(id), max_index, next_is_dynamic(c, id+1));
        last_id = id;
    }
    draw_dynamic(size == 0 ? 0 : ids.back(), TILE_COUNT);
    do_draw(size, nullptr, max_index, true);

#ifdef FM_DEBUG_DRAW_COUNT
    if (draw_count)
        fm_debug("anim draws: %zu", draw_count);
#endif
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, std::size_t frame, const Vector3& center, float depth)
{
    const auto pos = atlas.frame_quad(center, r, frame);
    const auto& g = atlas.group(r);
    const auto texcoords = atlas.texcoords_for_frame(r, frame, !g.mirror_from.isEmpty());
    quad_data array;
    for (std::size_t i = 0; i < 4; i++)
        array[i] = { pos[i], texcoords[i], depth };
    _vertex_buffer.setSubData(0, array);
    atlas.texture().bind(0);
    shader.draw(_mesh);
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, std::size_t frame, local_coords xy)
{
    const auto pos = Vector3(xy.x, xy.y, 0.f) * TILE_SIZE;
    const float depth = tile_shader::depth_value(xy, tile_shader::scenery_depth_offset);
    draw(shader, atlas, r, frame, pos, depth);
}


} // namespace floormat
