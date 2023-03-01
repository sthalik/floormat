#include "anim.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "shaders/tile.hpp"
#include "main/clickable.hpp"
#include <Corrade/Containers/Optional.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Texture.h>

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
                              std::vector<clickable>& list)
{
    const local_coords xy{i};
    const auto& a = *atlas;
    const auto& g = a.group(s.r);
    const auto& f = a.frame(s.r, s.frame);
    const auto world_pos = TILE_SIZE20 * Vector3(xy) + Vector3(g.offset) + Vector3(Vector2(s.offset), 0);
    const Vector2i offset((Vector2(shader.camera_offset()) + Vector2(win_size)*.5f)
                          + shader.project(world_pos) - Vector2(f.ground));
    if (offset < win_size && offset + Vector2i(f.size) >= Vector2i())
    {
        clickable item = {
            { f.offset, f.offset + f.size }, { offset, offset + Vector2i(f.size) },
            a.bitmask(), tile_shader::depth_value(xy, tile_shader::scenery_depth_offset),
            a.info().pixel_size[0],
            c, xy,
            !g.mirror_from.isEmpty(),
        };
        list.push_back(item);
    }
}

void anim_mesh::draw(tile_shader& shader, chunk& c)
{
    constexpr auto quad_index_count = 6;
    auto [mesh_, ids, size] = c.ensure_scenery_mesh();
    GL::MeshView mesh{mesh_};
    anim_atlas* bound = nullptr;

    [[maybe_unused]] std::size_t draw_count = 0;
    fm_debug_assert(std::size_t(mesh_.count()) == size*quad_index_count);

    const auto do_draw = [&](std::size_t from, std::size_t to, anim_atlas* atlas, std::uint32_t max_index) {
        if (atlas != bound)
            atlas->texture().bind(0);
        bound = atlas;
        mesh.setCount((int)(quad_index_count * (to-from)));
        mesh.setIndexRange((int)(from*quad_index_count), 0, max_index);
        shader.draw(mesh);
        draw_count++;
    };

    struct last_ { anim_atlas* atlas = nullptr; std::size_t run_from = 0; };
    Optional<last_> last;
    const auto max_index = std::uint32_t(size*quad_index_count - 1);

    auto last_id = 0_uz;
    for (auto k = 0_uz; k < size; k++)
    {
        auto id = ids[k];
        auto [atlas, s] = c[id].scenery();
        for (auto i = last_id+1; i < id; i++)
            if (auto [atlas, s] = c[i].scenery();
                atlas && atlas->info().fps > 0)
            {
                if (last)
                {
                    do_draw(last->run_from, k, last->atlas, max_index);
                    last = NullOpt;
                }
                bound = nullptr;
                draw(shader, *atlas, s.r, s.frame, local_coords{i}, s.offset);
            }
        last_id = id;
        if (last && atlas && &*atlas != last->atlas)
        {
            do_draw(last->run_from, k, last->atlas, max_index);
            last = NullOpt;
        }
        if (!last)
            last = { InPlaceInit, &*atlas, k };
    }
    if (size > 0)
    {
        if (last)
            do_draw(last->run_from, size, last->atlas, max_index);
        for (std::size_t i = ids[size-1]+1; i < TILE_COUNT; i++)
            if (auto [atlas, s] = c[i].scenery(); atlas && atlas->info().fps > 0)
                draw(shader, *atlas, s.r, s.frame, local_coords{i}, s.offset);
    }
    else
        for (auto i = 0_uz; i < TILE_COUNT; i++)
            if (auto [atlas, s] = c[i].scenery(); atlas)
                draw(shader, *atlas, s.r, s.frame, local_coords{i}, s.offset);

//#define FM_DEBUG_DRAW_COUNT
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
    for (auto i = 0_uz; i < 4; i++)
        array[i] = { pos[i], texcoords[i], depth };
    _vertex_buffer.setSubData(0, array);
    atlas.texture().bind(0);
    shader.draw(_mesh);
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, std::size_t frame, local_coords xy, Vector2b offset)
{
    const auto pos = Vector3(xy) * TILE_SIZE + Vector3(Vector2(offset), 0);
    const float depth = tile_shader::depth_value(xy, tile_shader::scenery_depth_offset);
    draw(shader, atlas, r, frame, pos, depth);
}


} // namespace floormat
