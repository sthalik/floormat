#include "chunk-scenery.hpp"
#include "shaders/tile.hpp"
#include "entity.hpp"
#include "anim-atlas.hpp"
#include "tile-atlas.hpp"
#include <Corrade/Containers/ArrayViewStl.h>
#include <Magnum/GL/Buffer.h>
#include <algorithm>

namespace floormat {

auto chunk::ensure_scenery_mesh(Array<draw_entity>&& array) noexcept -> scenery_mesh_tuple
{
    return ensure_scenery_mesh(static_cast<Array<draw_entity>&>(array));
}

bool operator<(const chunk::topo_sort_data& a, const chunk::topo_sort_data& b)
{

}

bool chunk::topo_sort_data::intersects(const topo_sort_data& o) const
{
    return min[0] <= o.max[0] && max[0] >= o.min[0] &&
           min[1] <= o.max[1] && max[1] >= o.min[1];
}

auto chunk::make_topo_sort_data(const entity& e) -> topo_sort_data
{
    const auto& a = *e.atlas;
    const auto& g = a.group(e.r);
    const auto& f = a.frame(e.r, e.frame);
    const auto world_pos = TILE_SIZE20 * Vector3(e.coord.local()) + Vector3(g.offset) + Vector3(Vector2(e.offset), 0);
    const auto px_start = tile_shader::project(world_pos) - Vector2(f.ground), px_end = px_start + Vector2(f.size);
    topo_sort_data data = {
        .min = Vector2i(px_start), .max = Vector2i(px_end),
        .center = data.min + (data.max - data.min)/2,
        .ord = e.ordinal(),
        .is_character = false,
    };
    if (e.type() == entity_type::scenery && !e.is_dynamic())
    {
        const auto bb_min_ = world_pos - Vector3(Vector2(e.bbox_size/2) + Vector2(e.bbox_offset), 0);
        const auto bb_max_ = bb_min_ + Vector3(Vector2(e.bbox_size), 0);
        Vector2 start, end;
        switch (e.r)
        {
        using enum rotation;
        case N:
        case S:
            start = Vector2(bb_min_[0], bb_min_[1]);
            end = Vector2(bb_max_[0], bb_max_[1]);
            break;
        case W:
        case E:
            start = Vector2(bb_min_[0], bb_max_[1]);
            end = Vector2(bb_max_[0], bb_min_[1]);
            break;
        default:
            break;
        }
        const auto bb_min = tile_shader::project(Vector3(start, 0));
        const auto bb_max = tile_shader::project(Vector3(end, 0));
        const auto bb_len = std::fabs(bb_max[0] - bb_min[0]);
        if (bb_len >= 1)
            data.slope = (bb_max[1]-bb_min[1])/bb_len;
    }
    else if (e.type() == entity_type::character)
        data.is_character = true;
    return data;
}

auto chunk::ensure_scenery_mesh(Array<draw_entity>& array) noexcept -> scenery_mesh_tuple
{
    constexpr auto entity_ord_lessp = [](const auto& a, const auto& b) {
      return a.ord < b.ord;
    };

    fm_assert(_entities_sorted);

    const auto size = _entities.size();

    ensure_scenery_draw_array(array);
    for (auto i = 0uz; const auto& e : _entities)
        array[i++] = { e.get(), e->ordinal(), make_topo_sort_data(*e) };
    std::sort(array.begin(), array.begin() + size, entity_ord_lessp);

    const auto es = ArrayView<draw_entity>{array, size};

    if (_scenery_modified)
    {
        _scenery_modified = false;

        const auto count = fm_begin(
            size_t ret = 0;
            for (const auto& [e, ord, _data] : es)
                ret += !e->is_dynamic();
            return ret;
        );

        scenery_indexes.clear();
        scenery_indexes.reserve(count);
        scenery_vertexes.clear();
        scenery_vertexes.reserve(count);

        for (const auto& [e, ord, _data] : es)
        {
            if (e->is_dynamic())
                continue;

            const auto i = scenery_indexes.size();
            scenery_indexes.emplace_back();
            scenery_indexes.back() = tile_atlas::indices(i);
            const auto& atlas = e->atlas;
            const auto& fr = *e;
            const auto pos = e->coord.local();
            const auto coord = Vector3(pos) * TILE_SIZE + Vector3(Vector2(fr.offset), 0);
            const auto quad = atlas->frame_quad(coord, fr.r, fr.frame);
            const auto& group = atlas->group(fr.r);
            const auto texcoords = atlas->texcoords_for_frame(fr.r, fr.frame, !group.mirror_from.isEmpty());
            const float depth = tile_shader::depth_value(pos, tile_shader::scenery_depth_offset);
            scenery_vertexes.emplace_back();
            auto& v = scenery_vertexes.back();
            for (auto j = 0uz; j < 4; j++)
                v[j] = { quad[j], texcoords[j], depth };
        }

        GL::Mesh mesh{GL::MeshPrimitive::Triangles};
        mesh.addVertexBuffer(GL::Buffer{scenery_vertexes}, 0, tile_shader::Position{}, tile_shader::TextureCoordinates{}, tile_shader::Depth{})
            .setIndexBuffer(GL::Buffer{scenery_indexes}, 0, GL::MeshIndexType::UnsignedShort)
            .setCount(int32_t(6 * count));
        scenery_mesh = Utility::move(mesh);
    }

    fm_assert(!size || es);

    return { scenery_mesh, es, size };
}

void chunk::ensure_scenery_draw_array(Array<draw_entity>& array)
{
    const size_t len_ = _entities.size();

    if (len_ <= array.size())
        return;

    size_t len;

    if (len_ > 1 << 17)
        len = len_;
    else
        len = std::bit_ceil(len_);

    array = Array<draw_entity>{len};
}

} // namespace floormat
