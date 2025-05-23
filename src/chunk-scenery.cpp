#include "chunk-scenery.hpp"
#include "tile-constants.hpp"
#include "shaders/shader.hpp"
#include "object.hpp"
#include "anim-atlas.hpp"
#include "scenery.hpp"
#include "quads.hpp"
#include <bit>
#include <Corrade/Containers/ArrayView.h>
#include <Magnum/GL/Buffer.h>

namespace floormat {

using namespace floormat::Quads;
using topo_sort_data = chunk::topo_sort_data;

namespace {

auto make_topo_sort_data(object& e, uint32_t mesh_idx) -> topo_sort_data
{
    const auto& a = *e.atlas;
    const auto& f = a.frame(e.r, e.frame);
    const auto world_pos = TILE_SIZE20 * Vector3(e.coord.local()) + Vector3(Vector2(e.offset) + Vector2(e.bbox_offset), 0);
    const auto pos = tile_shader::project(world_pos);
    const auto px_start = pos - Vector2(e.bbox_offset) - Vector2(f.ground), px_end = px_start + Vector2(f.size);
    topo_sort_data data = {
        .in = &e,
        .min = Vector2i(px_start),
        .max = Vector2i(px_end),
        .center = Vector2i(pos),
        .in_mesh_idx = mesh_idx,
        .ord = e.ordinal(),
    };
    if (e.type() == object_type::scenery && !e.is_dynamic())
    {
        const auto bb_min_ = world_pos - Vector3(Vector2(e.bbox_size/2), 0);
        const auto bb_max_ = bb_min_ + Vector3(Vector2(e.bbox_size), 0);
        const auto& sc = static_cast<scenery&>(e);
        switch (e.r)
        {
        using enum rotation;
        default:
            break;
        case N:
        case S:
        case W:
        case E:
            const auto bb_min = tile_shader::project(Vector3(Vector2(bb_min_[0], bb_max_[1]), 0));
            const auto bb_max = tile_shader::project(Vector3(Vector2(bb_max_[0], bb_min_[1]), 0));
            const auto bb_len = bb_max[0] - bb_min[0];
            if (bb_len >= 1 && f.size[0] > (unsigned)iTILE_SIZE[0])
            {
                data.slope = (bb_max[1]-bb_min[1])/bb_len;
                data.bb_min = Vector2s(bb_min - px_start);
                data.bb_max = Vector2s(bb_max - px_start);
                if (sc.scenery_type() != scenery_type::door)
                    data.mode = topo_sort_data::mode_static;
            }
            break;
        }
    }
    else if (e.type() == object_type::critter)
        data.mode = topo_sort_data::mode_character;
    return data;
}

// todo! switch to using a stack as in https://www.geeksforgeeks.org/topological-sorting/

void topo_dfs(Array<chunk::object_draw_order>& array, size_t& output, size_t i, size_t size) // NOLINT(misc-no-recursion)
{
    using m = typename chunk::topo_sort_data::m;

    if (array[i].data.visited)
        return;
    array[i].data.visited = true;

    const auto& data_i = array[i].data;

    for (auto j = 0u; j < size; j++)
    {
        if (i == j)
            continue;
        const auto& data_j = array[j].data;
        if (data_j.visited)
            continue;
        if (data_j.mode == m::mode_static && data_i.mode == m::mode_character)
        {
            if (!data_i.intersects(data_j))
                continue;
            const auto &c = data_i, &s = data_j;
            auto off = c.center.x() - s.center.x();
            auto y = s.center.y() + s.slope * off;
            if (y < c.center.y())
                topo_dfs(array, output, j, size);
        }
        else if (data_i.mode == m::mode_static && data_j.mode == m::mode_character)
        {
            if (!data_i.intersects(data_j))
                continue;
            const auto &c = data_j, &s = data_i;
            auto off = c.center.x() - s.center.x();
            auto y = s.center.y() + s.slope * off;
            if (y >= c.center.y())
                topo_dfs(array, output, j, size);
        }
        else if (data_i.ord > data_j.ord)
            topo_dfs(array, output, j, size);
    }
    fm_assert(output < size);
    array[output].e = data_i.in;
    array[output].mesh_idx = data_i.in_mesh_idx;
    output++;
}

void topological_sort(Array<chunk::object_draw_order>& array, size_t size)
{
    size_t output = 0;

    for (auto i = 0u; i < size; i++)
        if (!array[i].data.visited)
            topo_dfs(array, output, i, size);
    fm_assert(output == size);
}

} // namespace

auto chunk::ensure_scenery_mesh() noexcept -> scenery_mesh_tuple
{
    Array<object_draw_order> array;
    Array<std::array<vertex, 4>> scenery_vertexes;
    Array<std::array<UnsignedShort, 6>> scenery_indexes;
    return ensure_scenery_mesh({array, scenery_vertexes, scenery_indexes});
}

bool chunk::topo_sort_data::intersects(const topo_sort_data& o) const
{
    return min.x() <= o.max.x() && max.x() >= o.min.x() &&
           min.y() <= o.max.y() && max.y() >= o.min.y();
}

auto chunk::ensure_scenery_mesh(scenery_scratch_buffers buffers) noexcept -> scenery_mesh_tuple
{
    ensure_scenery_buffers(buffers);

    fm_assert(_objects_sorted);

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
            const auto d = e->depth_offset();
            const float depth = tile_shader::depth_value(pos, d);

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
        array[i++] = { e.get(), (uint32_t)-1, e->ordinal(), make_topo_sort_data(*e, index) };
    }
    topological_sort(array, i);

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
