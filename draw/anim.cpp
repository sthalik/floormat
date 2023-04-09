#include "anim.hpp"
#include "anim-atlas.hpp"
#include "chunk.hpp"
#include "shaders/tile.hpp"
#include "main/clickable.hpp"
#include "chunk-scenery.hpp"
#include <cstdio>
#include <Corrade/Containers/Optional.h>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

anim_mesh::anim_mesh()
{
    _mesh.setCount(quad_index_count)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());

    _batch_mesh.setCount(batch_size * quad_index_count)
               .addVertexBuffer(_batch_vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
               .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_batch_mesh.isIndexed());
}

auto anim_mesh::make_batch_index_array() -> std::array<std::array<UnsignedShort, quad_index_count>, batch_size>
{
    std::array<std::array<UnsignedShort, quad_index_count>, batch_size> array; // NOLINT(cppcoreguidelines-pro-type-member-init)
    for (std::size_t N = 0; N < batch_size; N++)
    {
        using u16 = uint16_t;
        array[N] = {                                    /* 3--1  1 */
            (u16)(0+N*4), (u16)(1+N*4), (u16)(2+N*4),   /* | /  /| */
            (u16)(2+N*4), (u16)(1+N*4), (u16)(3+N*4),   /* |/  / | */
        };                                              /* 2  2--0 */
    }
    return array;
}

std::array<UnsignedShort, 6> anim_mesh::make_index_array()
{
    return {{
        0, 1, 2,
        2, 1, 3,
    }};
}

void anim_mesh::add_clickable(tile_shader& shader, const Vector2i& win_size,
                              entity* s_, const chunk::topo_sort_data& data,
                              std::vector<clickable>& list)
{
    const auto& s = *s_;
    const auto& a = *s.atlas;
    const auto& g = a.group(s.r);
    const auto& f = a.frame(s.r, s.frame);
    const auto world_pos = TILE_SIZE20 * Vector3(s.coord.local()) + Vector3(g.offset) + Vector3(Vector2(s.offset), 0);
    const Vector2i offset((Vector2(shader.camera_offset()) + Vector2(win_size)*.5f)
                          + shader.project(world_pos) - Vector2(f.ground));
    if (offset < win_size && offset + Vector2i(f.size) >= Vector2i())
    {
        clickable item = {
            .src = { f.offset, f.offset + f.size },
            .dest = { offset, offset + Vector2i(f.size) },
            .bitmask = a.bitmask(),
            .e = s_,
            .depth = s.ordinal() + (float)s.coord.z() * TILE_COUNT,
            .slope = data.slope,
            .bb_min = data.bb_min, .bb_max = data.bb_max,
            .stride = a.info().pixel_size[0],
            .mirrored = !g.mirror_from.isEmpty(),
        };
        list.push_back(item);
    }
}

void anim_mesh::draw(tile_shader& shader, const Vector2i& win_size, chunk& c, std::vector<clickable>& list)
{
    GL::MeshView mesh{_batch_mesh};
    auto [es] = c.ensure_scenery_mesh(_draw_array);
    std::array<quad_data, batch_size> array = {};
    std::array<anim_atlas*, batch_size> atlases = {};
    anim_atlas* last_atlas = nullptr;

    constexpr auto do_draw = [](tile_shader& shader, GL::MeshView& mesh, anim_atlas*& last_atlas, anim_atlas* atlas, uint32_t i) {
          constexpr auto max_index = uint32_t(batch_size*quad_index_count - 1);
          if (atlas != last_atlas)
          {
              last_atlas = atlas;
              atlas->texture().bind(0);
          }
          mesh.setCount((int)(quad_index_count));
          mesh.setIndexRange((int)(i*quad_index_count), 0, max_index);
          shader.draw(mesh);
    };

    uint32_t k = 0;

    for (const auto& x : es)
    {
        fm_assert(x.e);
        add_clickable(shader, win_size, x.data.in, x.data, list);
        const auto& e = *x.e;
        const auto depth0 = e.depth_offset();
        const auto depth1 = depth0[1]*TILE_MAX_DIM + depth0[0];
        const auto depth = tile_shader::depth_value(e.coord.local(), depth1);
        const auto pos0 = Vector3(e.coord.local()) * TILE_SIZE + Vector3(Vector2(e.offset), 0);
        auto& atlas = *e.atlas;
        const auto pos = atlas.frame_quad(pos0, e.r, e.frame);
        const auto& g = atlas.group(e.r);
        const auto texcoords = atlas.texcoords_for_frame(e.r, e.frame, !g.mirror_from.isEmpty());

        for (auto i = 0uz; i < 4; i++)
            array[k][i] = { pos[i], texcoords[i], depth };
        atlases[k] = &atlas;

        if (++k >= 1)
        {
            _batch_vertex_buffer.setSubData(0, { array.data(), k });
            for (uint32_t i = 0; i < k; i++)
                do_draw(shader, mesh, last_atlas, atlases[i], i);
            k = 0;
        }
    }

    if (k > 0)
    {
        _batch_vertex_buffer.setSubData(0, ArrayView<const quad_data>{array.data(), k});
        for (uint32_t i = 0; i < k; i++)
            do_draw(shader, mesh, last_atlas, atlases[i], i);
    }
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, const Vector3& center, float depth)
{
    const auto pos = atlas.frame_quad(center, r, frame);
    const auto& g = atlas.group(r);
    const auto texcoords = atlas.texcoords_for_frame(r, frame, !g.mirror_from.isEmpty());
    quad_data array;
    for (auto i = 0uz; i < 4; i++)
        array[i] = { pos[i], texcoords[i], depth };
    _vertex_buffer.setSubData(0, array);
    atlas.texture().bind(0);
    shader.draw(_mesh);
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, local_coords xy, Vector2b offset, float depth)
{
    const auto pos = Vector3(xy) * TILE_SIZE + Vector3(Vector2(offset), 0);
    draw(shader, atlas, r, frame, pos, depth);
}


} // namespace floormat
