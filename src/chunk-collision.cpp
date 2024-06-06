#include "chunk.hpp"
#include "ground-atlas.hpp"
#include "object.hpp"
#include "src/RTree-search.hpp"
#include "rect-intersects.hpp"
#include "src/chunk-scenery.hpp"
#include "src/tile-bbox.hpp"
#include "src/hole.hpp"
#include "src/wall-atlas.hpp"
#include <bit>
#include <Corrade/Containers/StructuredBindings.h>
#include <Corrade/Containers/Pair.h>
//#include <cr/GrowableArray.h>

namespace floormat {

bool collision_data::operator==(const collision_data&) const noexcept = default;
bool chunk::bbox::operator==(const floormat::chunk::bbox& other) const noexcept = default;
chunk::RTree* chunk::rtree() noexcept { ensure_passability(); return &*_rtree; }
world& chunk::world() noexcept { return *_world; }

namespace {

constexpr collision_data make_id_(collision_type type, pass_mode p, object_id id)
{
    return collision_data { (object_id)type, (object_id)p, id };
}

constexpr object_id make_id(collision_type type, pass_mode p, object_id id)
{
    fm_debug_assert(id < object_id{1} << collision_data_BITS);
    return std::bit_cast<object_id>(make_id_(type, p, id));
}

struct Data
{
    object_id id;
    Vector2 min, max;
};

#if 0
template<bool IsNeighbor>
void add_holes_from(chunk::RTree& rtree, chunk& c, Vector2b chunk_offset)
{
    constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
    constexpr auto max_bbox_size = Vector2i{0xff, 0xff};
    constexpr auto chunk_min = -iTILE_SIZE2/2 - max_bbox_size/2,
                   chunk_max = TILE_MAX_DIM * iTILE_SIZE2 - iTILE_SIZE2 / 2 + max_bbox_size;
    for (const std::shared_ptr<object>& eʹ : c.objects())
    {
        if (eʹ->type() != object_type::hole) [[likely]]
            continue;
        const auto& e = static_cast<struct hole&>(*eʹ);
        auto center = Vector2i(e.offset) + Vector2i(e.bbox_offset) + Vector2i(e.coord.local()) * TILE_SIZE2;
        if constexpr(IsNeighbor)
        {
            const auto off = Vector2i(chunk_offset)*chunk_size;
            center += off;
        }
        const auto min = center - Vector2i(e.bbox_size/2), max = min + Vector2i(e.bbox_size);
        if constexpr(IsNeighbor)
            if (!rect_intersects(min, max, chunk_min, chunk_max)) [[likely]]
                continue;
        rtree.Insert(Vector2(min).data(), Vector2(max).data(), make_id(collision_type::none, pass_mode::pass, e.id));
    }
}

void filter_through_holes(chunk::RTree& rtree, Data bbox, unsigned stack)
{
    using Rect = typename chunk::RTree::Rect;
    Vector2 hmin, hmax;
    bool ret = true;
    rtree.Search(bbox.min.data(), bbox.max.data(), [&](uint64_t data, const Rect& r) {
        [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
        if (x.pass == (uint64_t)pass_mode::pass && x.tag == (uint64_t)collision_type::none)
        {
            hmin = Vector2(r.m_min[0], r.m_min[1]);
            hmax = Vector2(r.m_max[0], r.m_max[1]);
            return ret = false;
        }
        return true;
    });

    if (ret) [[likely]]
        rtree.Insert(bbox.min.data(), bbox.max.data(), bbox.id);
    else
    {
        //auto res = cut_rectangle_result<float>::cut();
        //for (auto i = 0uz; i < )
        fm_assert(++stack <= 4096);
    }
}
#else
void filter_through_holes(chunk::RTree& rtree, Data bbox, unsigned)
{
    rtree.Insert(bbox.min.data(), bbox.max.data(), bbox.id);
}
#endif

} // namespace

void chunk::ensure_passability() noexcept
{
    fm_assert(_objects_sorted); // not strictly necessary

    if (!_pass_modified)
        return;
    _pass_modified = false;

    _rtree->RemoveAll();

    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        if (const auto* atlas = ground_atlas_at(i))
        {
            auto [min, max] = whole_tile(i);
            auto id = make_id(collision_type::geometry, atlas->pass_mode(), i+1);
            filter_through_holes(*_rtree, {id, min, max}, 0);
        }
    }
    for (auto i = 0uz; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (const auto* atlas = tile.wall_north_atlas().get())
        {
            auto [min, max] = wall_north(i, (float)atlas->info().depth);
            auto id = make_id(collision_type::geometry, atlas->info().passability, TILE_COUNT+i+1);
            filter_through_holes(*_rtree, {id, min, max}, 0);

            if (tile.wall_west_atlas())
            {
                auto [min, max] = wall_pillar(i, (float)atlas->info().depth);
                filter_through_holes(*_rtree, {id, min, max}, 0);
            }
        }
        if (const auto* atlas = tile.wall_west_atlas().get())
        {
            auto [min, max] = wall_west(i, (float)atlas->info().depth);
            auto id = make_id(collision_type::geometry, atlas->info().passability, TILE_COUNT*2+i+1);
            filter_through_holes(*_rtree, {id, min, max}, 0);
        }
    }
    for (const std::shared_ptr<object>& s : objects())
        if (!s->is_dynamic())
            if (bbox box; _bbox_for_scenery(*s, box))
                filter_through_holes(*_rtree, {std::bit_cast<object_id>(box.data), Vector2(box.start), Vector2(box.end)}, 0);

    //for (auto [id, min, max] : vec) _rtree->Insert(min.data(), max.data(), id);
    //arrayResize(vec, 0); arrayResize(vec2, 0); // done with holes

    for (const std::shared_ptr<object>& s : objects())
    {
        bbox box;
        if (s->is_dynamic())
            if (_bbox_for_scenery(*s, box))
                _add_bbox(box);
    }
}

bool chunk::_bbox_for_scenery(const object& s, local_coords local, Vector2b offset,
                              Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept
{
    auto [start, end] = scenery_tile(local, offset, bbox_offset, bbox_size);
    auto id = make_id_(collision_type::scenery, s.pass, s.id);
    value = { .data = id, .start = start, .end = end };
    return Vector2ui(s.bbox_size).product() > 0 && s.atlas;
}

bool chunk::_bbox_for_scenery(const object& s, bbox& value) noexcept
{
    return _bbox_for_scenery(s, s.coord.local(), s.offset, s.bbox_offset, s.bbox_size, value);
}

void chunk::_remove_bbox(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree->Remove(start.data(), end.data(), std::bit_cast<object_id>(x.data));
}

void chunk::_add_bbox(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree->Insert(start.data(), end.data(), std::bit_cast<object_id>(x.data));
}

void chunk::_replace_bbox(const bbox& x0, const bbox& x1, bool b0, bool b1)
{
    if (_pass_modified)
        return;

    unsigned i = (unsigned)b1 << 1 | (unsigned)b0 << 0;
    CORRADE_ASSUME(i < 4u); (void)0;

    switch (i)
    {
    case 1 << 1 | 1 << 0:
        if (x1 == x0)
            return;
        _remove_bbox(x0);
        [[fallthrough]];
    case 1 << 1 | 0 << 0:
        _add_bbox(x1);
        return;
    case 0 << 1 | 1 << 0:
        _remove_bbox(x0);
        return;
    case 0 << 1 | 0 << 0:
        return;
    default:
        break;
    }
    std::unreachable();
}

bool chunk::can_place_object(const object_proto& proto, local_coords pos)
{
    (void)ensure_scenery_mesh();

    fm_assert(proto.pass < pass_mode::COUNT);
    switch (proto.pass)
    {
    case pass_mode::COUNT: std::unreachable();
    case pass_mode::blocked:
    case pass_mode::see_through:
        break;
    case pass_mode::pass:
    case pass_mode::shoot_through:
        return true;
    }

    if (!proto.bbox_size.x() || proto.bbox_size.y())
        return true;

    auto bbox_size = Vector2i(proto.bbox_size);
    if (bbox_size.x() > 1)
        bbox_size.x() -= 1;
    if (bbox_size.y() > 1)
        bbox_size.y() -= 1;

    const auto center = Vector2(pos)*TILE_SIZE2 + Vector2(proto.offset) + Vector2(proto.bbox_offset),
               min = center - Vector2(bbox_size)*.5f, max = min + Vector2(bbox_size);
    bool ret = true;
    _rtree->Search(min.data(), max.data(), [&](uint64_t data, const auto&) {
          [[maybe_unused]] auto x = std::bit_cast<collision_data>(data);
          if (x.pass == (uint64_t)pass_mode::pass || x.pass == (uint64_t)pass_mode::shoot_through)
              return true;
          return ret = false;
    });
    return ret;
}

} // namespace floormat
