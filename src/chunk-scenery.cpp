#include "chunk-scenery.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"
#include "object.hpp"
#include "anim-atlas.hpp"
#include "scenery.hpp"
#include "quads.hpp"
#include "depth.hpp"
#include "renderer.hpp"
#include <bit>
#include <algorithm>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

using namespace floormat::Quads;

auto chunk::ensure_scenery_mesh() noexcept -> scenery_mesh_tuple
{
    Array<object_draw_order> array;
    Array<std::array<vertex, 4>> scenery_vertexes;
    Array<std::array<UnsignedShort, 6>> scenery_indexes;
    return ensure_scenery_mesh({array, scenery_vertexes, scenery_indexes});
}

auto chunk::ensure_scenery_mesh(scenery_scratch_buffers buffers) noexcept -> scenery_mesh_tuple
{
    ensure_scenery_buffers(buffers);

    fm_assert(_objects_sorted);

    const float depth_start = Render::get_status().is_clipdepth01_enabled ? 0.f : -1.f;

    if (_scenery_modified)
    {
        _scenery_modified = false;

        const auto count = [this] {
            uint32_t ret = 0;
            for (const auto& e : _objects)
                ret += !e->is_dynamic();
            return ret;
        }();

        auto& scenery_vertexes = buffers.scenery_vertexes;
        auto& scenery_indexes = buffers.scenery_indexes;

        for (auto i = 0u; const auto& e : _objects)
        {
            if (e->is_dynamic())
                continue;
            const auto& atlas = e->atlas;
            fm_debug_assert(atlas != nullptr);
            const auto& fr = *e;
            const auto pos = e->coord.local();
            const auto coord = Vector3(pos) * TILE_SIZE + Vector3(Vector2(fr.offset), 0);
            const auto quad = atlas->frame_quad(coord, fr.r, fr.frame);
            const auto& group = atlas->group(fr.r);
            const auto texcoords = atlas->texcoords_for_frame(fr.r, fr.frame, !group.mirror_from.isEmpty());
            const float depth = Depth::value_at(depth_start, e->position(), e->depth_offset());

            for (auto j = 0u; j < 4; j++)
                scenery_vertexes[i][j] = { quad[j], texcoords[j], depth };
            scenery_indexes[i] = quad_indexes(i);
            i++;
        }

        if (count == 0)
            scenery_mesh = GL::Mesh{NoCreate};
        else
        {
            GL::Mesh mesh{GL::MeshPrimitive::Triangles};
            auto vert_view = ArrayView<const std::array<vertex, 4>>{scenery_vertexes, count};
            auto index_view = ArrayView<const std::array<UnsignedShort, 6>>{scenery_indexes, count};
            mesh.addVertexBuffer(GL::Buffer{vert_view}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
                .setIndexBuffer(GL::Buffer{index_view}, 0, GL::MeshIndexType::UnsignedShort)
                .setCount(int32_t(6 * count));
            scenery_mesh = move(mesh);
        }
    }

    const auto size = _objects.size();
    auto& array = buffers.array;
    uint32_t j = 0, i = 0;
    for (const auto& e : _objects)
    {
        auto index = e->is_dynamic() ? (uint32_t)-1 : j++;
        array[i++] = { e.get(), index };
    }
    std::sort(array.data(), array.data() + i,
              [d=depth_start](const object_draw_order& a, const object_draw_order& b) {
                  return Depth::value_at(d, a.e->position()) < Depth::value_at(d, b.e->position());
              });

    return { scenery_mesh, ArrayView<object_draw_order>{array, size}, j };
}

void chunk::ensure_scenery_buffers(scenery_scratch_buffers bufs)
{
    const size_t lenʹ = _objects.size();

    if (lenʹ <= bufs.array.size())
        return;

    size_t len;

    if (lenʹ > 1 << 20)
        len = lenʹ;
    else
        len = std::bit_ceil(lenʹ);

    bufs.array = Array<object_draw_order>{NoInit, len};
    bufs.scenery_vertexes = Array<std::array<vertex, 4>>{NoInit, len};
    bufs.scenery_indexes = Array<std::array<UnsignedShort, 6>>{NoInit, len};
}

} // namespace floormat
