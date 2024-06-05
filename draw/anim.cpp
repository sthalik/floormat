#include "anim.hpp"
#include "src/tile-constants.hpp"
#include "src/anim-atlas.hpp"
#include "src/chunk.hpp"
#include "shaders/shader.hpp"
#include "main/clickable.hpp"
#include "src/chunk-scenery.hpp"
#include "src/scenery.hpp"
#include <Corrade/Containers/GrowableArray.h>
#include <Corrade/Containers/ArrayViewStl.h>
#include <Corrade/Containers/Optional.h>
#include <Magnum/GL/MeshView.h>
#include <Magnum/GL/Texture.h>

namespace floormat {

namespace {

std::array<UnsignedShort, 6> make_index_array()
{
    return {{
        0, 1, 2,
        2, 1, 3,
    }};
}

struct minmax_s { uint32_t len, min; };

bool check_contig_draw(ArrayView<const chunk::object_draw_order> array, uint32_t start, uint32_t end, minmax_s& ret)
{
    const auto& e0 = array[start];
    uint32_t min = e0.mesh_idx, max = min;

    for (auto i = start+1; i < end; i++)
    {
        const auto& e = array[i];
        min = Math::min(min, e.mesh_idx);
        max = Math::max(max, e.mesh_idx);
    }

    auto len = max - min;
    auto sz = end - start;
    ret = {
        .len = sz,
        .min = min,
    };
    return len+1 == sz;
}

uint32_t get_contig_draw_max_len(ArrayView<const chunk::object_draw_order> array, uint32_t start)
{
    const auto size = (uint32_t)array.size();
    uint32_t len = 1;
    const auto* a0 = array[start].e->atlas.get();
    for (auto i = start+1; i < size; i++)
    {
        const auto& e = array[i];
        if (e.e->atlas.get() != a0)
            break;
        if (e.mesh_idx == (uint32_t)-1)
            break;
        len++;
    }
    return len;
}

minmax_s get_contig_draw_len(ArrayView<const chunk::object_draw_order> array, uint32_t start)
{
    auto r = minmax_s { .len = 1, .min = array[start].mesh_idx, };
    auto len = get_contig_draw_max_len(array, start);
    for (auto i = len; i > 1; i--)
        if (check_contig_draw(array, start, start + i, r))
            return r;
    return r;
}

} // namespace

anim_mesh::anim_mesh() :
    _vertex_buffer{quad_data{}, Magnum::GL::BufferUsage::DynamicDraw},
    _index_buffer{make_index_array()}
{
    _mesh.setCount(6)
        .addVertexBuffer(_vertex_buffer, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
        .setIndexBuffer(_index_buffer, 0, GL::MeshIndexType::UnsignedShort);
    CORRADE_INTERNAL_ASSERT(_mesh.isIndexed());
}

void anim_mesh::add_clickable(tile_shader& shader, const Vector2i& win_size,
                              object* sʹ, const chunk::topo_sort_data& data,
                              Array<clickable>& list)
{
    const auto& s = *sʹ;
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
            .e = sʹ,
            .depth = s.ordinal() + (float)s.coord.z() * TILE_COUNT,
            .slope = data.slope,
            .bb_min = data.bb_min, .bb_max = data.bb_max,
            .stride = a.info().pixel_size[0],
            .mirrored = !g.mirror_from.isEmpty(),
        };
        arrayAppend(list, item);
    }
}

void anim_mesh::draw(tile_shader& shader, const Vector2i& win_size, chunk& c, Array<clickable>& list, bool draw_vobjs)
{
    constexpr auto quad_index_count = 6;

    auto [mesh_, es, size] = c.ensure_scenery_mesh({ _draw_array, _draw_vertexes, _draw_indexes, });
    const auto max_index = uint32_t(size*quad_index_count - 1);

    uint32_t k = 0;

    while (k < es.size())
    {
        const auto& x = es[k];
        fm_assert(x.e);
        add_clickable(shader, win_size, x.data.in, x.data, list);
        auto& e = *x.e;

        auto& atlas = *e.atlas;
        fm_assert(e.is_dynamic() == (x.mesh_idx == (uint32_t)-1));
        if (!e.is_dynamic())
        {
#if 0 // todo! broken
            const auto r = get_contig_draw_len(es, k);
#else
            const auto r = minmax_s{1, x.mesh_idx};
#endif
            uint32_t count = r.len;
            GL::MeshView mesh{mesh_};
            mesh.setCount(quad_index_count * (Int)count);
            mesh.setIndexOffset((int)(r.min*quad_index_count), 0, max_index);
            shader.draw(atlas.texture(), mesh);
            //if (count > 1) Debug{} << "foo" << atlas.name() << count;
            k += count;
        }
        else
        {
            if (!draw_vobjs) [[likely]]
                if (e.is_virtual()) [[unlikely]]
                    continue;

            const auto depth0 = e.depth_offset();
            const auto depth = tile_shader::depth_value(e.coord.local(), depth0);
            draw(shader, atlas, e.r, e.frame, e.coord.local(), e.offset, depth);
            k++;
        }
    }
    fm_assert(k == es.size());
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
    shader.draw(atlas.texture(), _mesh);
}

void anim_mesh::draw(tile_shader& shader, anim_atlas& atlas, rotation r, size_t frame, local_coords xy, Vector2b offset, float depth)
{
    const auto pos = Vector3(xy) * TILE_SIZE + Vector3(Vector2(offset), 0);
    draw(shader, atlas, r, frame, pos, depth);
}


} // namespace floormat
