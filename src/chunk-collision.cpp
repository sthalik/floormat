#include "chunk.hpp"
#include "ground-atlas.hpp"
#include "object.hpp"
#include "world.hpp"
#include "src/RTree-search.hpp"
#include "rect-intersects.hpp"
#include "hole.hpp"
#include "src/chunk-scenery.hpp"
#include "src/tile-bbox.hpp"
#include "src/hole.hpp"
#include "src/wall-atlas.hpp"
#include <bit>
#include <utility>
#include <Corrade/Containers/StructuredBindings.h>
#include <Corrade/Containers/Pair.h>

namespace floormat {

bool collision_data::operator==(const collision_data&) const noexcept = default;
bool chunk::bbox::operator==(const floormat::chunk::bbox& other) const noexcept = default;
Chunk_RTree* chunk::rtree() noexcept { ensure_passability(); return &*_rtree; }
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

template<bool IsNeighbor>
bool add_holes_from_chunk(Chunk_RTree& rtree, chunk& c, Vector2b chunk_offset)
{
    bool has_holes = false;
    constexpr auto chunk_size = iTILE_SIZE2 * TILE_MAX_DIM;
    constexpr auto max_bbox_size = Vector2i{0x100};
    constexpr auto chunk_min = -iTILE_SIZE2/2 - max_bbox_size/2,
                   chunk_max = TILE_MAX_DIM * iTILE_SIZE2 - iTILE_SIZE2 / 2 + max_bbox_size;
    for (const bptr<object>& eʹʹ : c.objects())
    {
        auto& eʹ = *eʹʹ;
        if (eʹ.type() != object_type::hole) [[likely]]
            continue;
        const auto& e = static_cast<struct hole&>(eʹ);
        if (!e.flags.enabled | !e.flags.on_physics)
            continue;
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
        has_holes = true;
    }
    return has_holes;
}

void filter_bbox_through_holes(Chunk_RTree& rtree, object_id id, Vector2 min, Vector2 max, bool has_holes)
{
    if (!has_holes)
        return rtree.Insert(min.data(), max.data(), id);
start:
    fm_assert(min != max);

    CutResult<float>::rect hole;
    bool ret = chunk::find_hole_in_bbox(hole, rtree, min, max);

    if (ret) [[likely]]
        rtree.Insert(min.data(), max.data(), id);
    else
    {
        auto res = CutResult<float>::cut(min, max, hole.min, hole.max);
        if (!res.found())
        {
            rtree.Insert(min.data(), max.data(), id);
        }
        else if (res.size == 1)
        {
            min = res.array[0].min;
            max = res.array[0].max;
            goto start;
        }
        else
        {
            for (auto i = 0u; i < res.size; i++)
                filter_bbox_through_holes(rtree, id, res.array[i].min, res.array[i].max, has_holes);
        }
    }
}

} // namespace

#if 1
bool chunk::find_hole_in_bbox(CutResult<float>::rect& hole, const Chunk_RTree& rtree, Vector2 min, Vector2 max)
{
    bool ret = true;
    rtree.Search(min.data(), max.data(), [&](uint64_t data, const Chunk_RTree::Rect& r) {
        auto x = std::bit_cast<collision_data>(data);
        if (x.pass == (uint64_t)pass_mode::pass && x.type == (uint64_t)collision_type::none)
        {
            CutResult<float>::rect holeʹ {
                .min = { r.m_min[0], r.m_min[1] },
                .max = { r.m_max[0], r.m_max[1] },
            };
            if (rect_intersects(holeʹ.min, holeʹ.max, min, max))
            {
                hole = holeʹ;
                return ret = false;
            }
        }
        return true;
    });
    return ret;
}
#else
bool chunk::find_hole_in_bbox(CutResult<float>::rect&, Chunk_RTree&, Vector2, Vector2) { return true; }
#endif
bool chunk::find_hole_in_bbox(CutResult<float>::rect& hole, Vector2 min, Vector2 max) { return find_hole_in_bbox(hole, *rtree(), min, max); }

void chunk::ensure_passability() noexcept
{
    fm_assert(_objects_sorted); // not strictly necessary

    if (!_pass_modified)
        return;
    _pass_modified = false;

    _rtree->RemoveAll();
    //Debug{} << ".. reset passability" << _coord;

    bool has_holes = false;
    auto& rtree = *_rtree;
    {
        has_holes |= add_holes_from_chunk<false>(rtree, *this, {});
        const auto nbs = _world->neighbors(_coord);
        for (auto i = 0u; i < 8; i++)
            if (nbs[i])
                has_holes |= add_holes_from_chunk<true>(rtree, *nbs[i], world::neighbor_offsets[i]);
    }

    for (auto i = 0u; i < TILE_COUNT; i++)
    {
        if (const auto* atlas = ground_atlas_at(i))
        {
            auto [min, max] = whole_tile(i);
            auto pass = atlas->pass_mode();
            if (pass == pass_mode::pass) [[likely]]
                continue;
            auto id = make_id(collision_type::geometry, pass, i+1);
            filter_bbox_through_holes(rtree, id, min, max, has_holes);
        }
    }
    for (auto i = 0u; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (const auto* atlas = tile.wall_north_atlas().get())
        {
            auto [min, max] = wall_north(i, (float)atlas->info().depth);
            auto id = make_id(collision_type::geometry, atlas->info().passability, TILE_COUNT+i+1);
            filter_bbox_through_holes(rtree, id, min, max, has_holes);

            if (tile.wall_west_atlas())
            {
                auto [min, max] = wall_pillar(i, (float)atlas->info().depth);
                filter_bbox_through_holes(rtree, id, min, max, has_holes);
            }
        }
        if (const auto* atlas = tile.wall_west_atlas().get())
        {
            auto [min, max] = wall_west(i, (float)atlas->info().depth);
            auto id = make_id(collision_type::geometry, atlas->info().passability, TILE_COUNT*2+i+1);
            filter_bbox_through_holes(rtree, id, min, max, has_holes);
        }
    }
    for (const bptr<object>& eʹ : objects())
    {
        if (eʹ->updates_passability())
            continue;
        bbox bb;
        if (_bbox_for_scenery(*eʹ, bb))
        {
            if (!eʹ->is_dynamic())
                filter_bbox_through_holes(rtree, std::bit_cast<object_id>(bb.data), Vector2(bb.start), Vector2(bb.end), has_holes);
            else
                _add_bbox_dynamic(bb);
        }
    }
    fm_assert(!_pass_modified);
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

void chunk::_remove_bbox_static_(const bptr<object>& e)
{
    mark_passability_modified();
    e->maybe_mark_neighbor_chunks_modified();
}

void chunk::_add_bbox_static_(const bptr<object>& e)
{
    mark_passability_modified();
    e->maybe_mark_neighbor_chunks_modified();
}

void chunk::_remove_bbox_(const bptr<object>& e, const bbox& x, bool upd, bool is_dynamic)
{
    if (!is_dynamic || upd)
        _remove_bbox_static(e, x);
    else
        _remove_bbox_dynamic(x);
}

void chunk::_remove_bbox_dynamic(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree->Remove(start.data(), end.data(), std::bit_cast<object_id>(x.data));
    //Debug{} << "bbox <<< dynamic" << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

void chunk::_remove_bbox_static(const bptr<object>& e, [[maybe_unused]] const bbox& x)
{
    _remove_bbox_static_(e);
    //Debug{} << "bbox <<< static " << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

void chunk::_add_bbox_dynamic(const bbox& x)
{
    auto start = Vector2(x.start), end = Vector2(x.end);
    _rtree->Insert(start.data(), end.data(), std::bit_cast<object_id>(x.data));
    //Debug{} << "bbox >>> dynamic" << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

void chunk::_add_bbox_static(const bptr<object>& e, [[maybe_unused]]const bbox& x)
{
    _add_bbox_static_(e);
    //Debug{} << "bbox >>> static " << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

void chunk::_add_bbox_(const bptr<object>& e, const bbox& x, bool upd, bool is_dynamic)
{
    if (!is_dynamic || upd)
        _add_bbox_static(e, x);
    else
        _add_bbox_dynamic(x);
}

template<bool Dynamic>
void chunk::_replace_bbox_impl(const bptr<object>& e, const bbox& x0, const bbox& x1, bool b0, bool b1)
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
        if constexpr(Dynamic)
            _remove_bbox_dynamic(x0);
        else
            _remove_bbox_static(e, x0);
        [[fallthrough]];
    case 1 << 1 | 0 << 0:
        if constexpr(Dynamic)
            _add_bbox_dynamic(x1);
        else
            _add_bbox_static(e, x1);
        return;
    case 0 << 1 | 1 << 0:
        if constexpr(Dynamic)
            _remove_bbox_dynamic(x0);
        else
            _remove_bbox_static(e, x0);
        return;
    case 0 << 1 | 0 << 0:
        return;
    default:
        break;
    }
    std::unreachable();
}

void chunk::_replace_bbox_dynamic(const bbox& x0, const bbox& x, bool b0, bool b)
{
    _replace_bbox_impl<true>(nullptr, x0, x, b0, b);
}

void chunk::_replace_bbox_static(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b)
{
    _replace_bbox_impl<false>(e, x0, x, b0, b);
}

void chunk::_replace_bbox_(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b, bool upd, bool is_dynamic)
{
    if (!is_dynamic || upd)
        _replace_bbox_static(e, x0, x, b0, b);
    else
        _replace_bbox_dynamic(x0, x, b0, b);
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
