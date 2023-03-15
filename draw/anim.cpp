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

void anim_mesh::add_clickable(tile_shader& shader, const Vector2i& win_size, const std::shared_ptr<entity>& s, std::vector<clickable>& list)
{
    const auto& a = *s->atlas;
    const auto& g = a.group(s->r);
    const auto& f = a.frame(s->r, s->frame);
    const auto world_pos = TILE_SIZE20 * Vector3(s->coord.local()) + Vector3(g.offset) + Vector3(Vector2(s->offset), 0);
    const Vector2i offset((Vector2(shader.camera_offset()) + Vector2(win_size)*.5f)
                          + shader.project(world_pos) - Vector2(f.ground));
    if (offset < win_size && offset + Vector2i(f.size) >= Vector2i())
    {
        clickable item = {
            { f.offset, f.offset + f.size }, { offset, offset + Vector2i(f.size) },
            a.bitmask(), s, s->ordinal(),
            a.info().pixel_size[0],
            !g.mirror_from.isEmpty(),
        };
        list.push_back(item);
    }
}

void anim_mesh::draw(tile_shader& shader, chunk& c)
{
    constexpr auto quad_index_count = 6;
    auto [mesh_] = c.ensure_scenery_mesh();
    const auto& es = c.entities();
    GL::MeshView mesh{mesh_};
    [[maybe_unused]] std::size_t draw_count = 0;

    const auto size = es.size();
    const auto max_index = std::uint32_t(size*quad_index_count - 1);

    const auto do_draw = [&](std::size_t from, std::size_t to, anim_atlas* atlas) {
        atlas->texture().bind(0);
        mesh.setCount((int)(quad_index_count * (to-from)));
        mesh.setIndexRange((int)(from*quad_index_count), 0, max_index);
        shader.draw(mesh);
        draw_count++;
    };

    fm_debug_assert(std::size_t(mesh_.count()) <= size*quad_index_count);

    struct last_ {
        anim_atlas* atlas = nullptr; std::size_t run_from = 0;
        operator bool() const { return atlas; }
        last_& operator=(std::nullptr_t) { atlas = nullptr; return *this; }
    } last;
    std::size_t i = 0;

    for (auto k = 0_uz; k < size; k++)
    {
        const auto& e = *es[k];
        auto& atlas = *e.atlas;
        if (last && &atlas != last.atlas)
        {
            do_draw(last.run_from, i, last.atlas);
            last = nullptr;
        }
        if (e.is_dynamic())
        {
            draw(shader, atlas, e.r, e.frame, e.coord.local(), e.offset, tile_shader::scenery_depth_offset);
            last = nullptr;
        }
        else
        {
            if (!last)
                last = { &atlas, i };
            i++;
        }
    }
    if (last)
        do_draw(last.run_from, i, last.atlas);

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

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, std::size_t frame, local_coords xy, Vector2b offset, float depth_offset)
{
    const auto pos = Vector3(xy) * TILE_SIZE + Vector3(Vector2(offset), 0);
    const float depth = tile_shader::depth_value(xy, depth_offset);
    draw(shader, atlas, r, frame, pos, depth);
}


} // namespace floormat
