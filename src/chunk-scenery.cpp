#include "chunk-scenery.hpp"
#include "shaders/tile.hpp"
#include "entity.hpp"
#include "anim-atlas.hpp"
#include "tile-atlas.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>
#include <algorithm>

namespace floormat {

auto chunk::ensure_scenery_mesh(Array<entity_draw_order>&& array) noexcept -> scenery_mesh_tuple
{
    return ensure_scenery_mesh(static_cast<Array<entity_draw_order>&>(array));
}

bool chunk::topo_sort_data::intersects(const topo_sort_data& o) const
{
    return min[0] <= o.max[0] && max[0] >= o.min[0] &&
           min[1] <= o.max[1] && max[1] >= o.min[1];
}

static void topo_dfs(Array<chunk::entity_draw_order>& array, size_t& output, size_t i, size_t size) // NOLINT(misc-no-recursion)
{
    using m = typename chunk::topo_sort_data::m;

    if (array[i].data.visited)
        return;
    array[i].data.visited = true;

    const auto& data_i = array[i].data;

    for (auto j = 0uz; j < size; j++)
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
    array[output++].e = data_i.in;
}

static void topological_sort(Array<chunk::entity_draw_order>& array, size_t size)
{
    size_t output = 0;

    for (auto i = 0uz; i < size; i++)
        if (!array[i].data.visited)
            topo_dfs(array, output, i, size);
    fm_assert(output == size);
}

auto chunk::make_topo_sort_data(entity& e) -> topo_sort_data
{
    const auto& a = *e.atlas;
    const auto& f = a.frame(e.r, e.frame);
    const auto world_pos = TILE_SIZE20 * Vector3(e.coord.local()) + Vector3(Vector2(e.offset) + Vector2(e.bbox_offset), 0);
    const auto pos = tile_shader::project(world_pos);
    const auto px_start = pos - Vector2(e.bbox_offset) - Vector2(f.ground), px_end = px_start + Vector2(f.size);
    topo_sort_data data = {
        .in = &e,
        .min = Vector2i(px_start), .max = Vector2i(px_end),
        .center = Vector2i(pos),
        .ord = e.ordinal(),
    };
    if (e.type() == entity_type::scenery && !e.is_dynamic())
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
            if (bb_len >= 1 && f.size[0] > uiTILE_SIZE[0])
            {
                data.slope = (bb_max[1]-bb_min[1])/bb_len;
                data.bb_min = Vector2s(bb_min - px_start);
                data.bb_max = Vector2s(bb_max - px_start);
                if (sc.sc_type != scenery_type::door)
                    data.mode = topo_sort_data::mode_static;
            }
            break;
        }
    }
    else if (e.type() == entity_type::character)
        data.mode = topo_sort_data::mode_character;
    return data;
}

auto chunk::ensure_scenery_mesh(Array<entity_draw_order>& array) noexcept -> scenery_mesh_tuple
{
    fm_assert(_entities_sorted);

    const auto size = _entities.size();

    ensure_scenery_draw_array(array);
    for (auto i = 0uz; const auto& e : _entities)
        array[i++] = { e.get(), e->ordinal(), make_topo_sort_data(*e) };
    std::sort(array.begin(), array.begin() + size, [](const auto& a, const auto& b) { return a.ord < b.ord; });
    topological_sort(array, size);

    return { ArrayView<entity_draw_order>{array, size} };
}

void chunk::ensure_scenery_draw_array(Array<entity_draw_order>& array)
{
    const size_t len_ = _entities.size();

    if (len_ <= array.size())
        return;

    size_t len;

    if (len_ > 1 << 17)
        len = len_;
    else
        len = std::bit_ceil(len_);

    array = Array<entity_draw_order>{len};
}

} // namespace floormat
