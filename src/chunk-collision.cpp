#include "chunk.hpp"
#include "object-storage.inl"
#include "ground-atlas.hpp"
#include "object.hpp"
#include "world.hpp"
#include "pass-through.hpp"
#include "src/search.hpp"
#include "RTree-search.hpp"
#include "rect-intersects.hpp"
#include "hole.hpp"
#include "hole-cut.hpp"
#include "src/tile-bbox.hpp"
#include "src/wall-atlas.hpp"
#include "compat/function2.hpp"
#include "compat/constantly.hpp"
#include <bit>
#include <utility>
#include <cr/StructuredBindings.h>
#include <cr/Optional.h>
#include <cr/Pair.h>
#include <mg/Range.h>

namespace floormat {

bool collision_data::operator==(const collision_data&) const noexcept = default;
bool chunk::bbox::operator==(const floormat::chunk::bbox& other) const noexcept = default;
Chunk_RTree* chunk::rtree() noexcept { ensure_passability(); return &*_rtree; }
Chunk_RTree* chunk::hole_rtree() noexcept { ensure_passability(); return &*_hole_rtree; }
Chunk_RTree* chunk::light_rtree() noexcept { ensure_passability(); return _light_rtree.get(); }
world& chunk::world() noexcept { return *_world; }
const class world& chunk::world() const noexcept { return *_world; }

const chunk::RTree* chunk::rtree() const noexcept
{
    fm_assert(!is_passability_modified());
    return _rtree.get();
}

const chunk::RTree* chunk::hole_rtree() const noexcept
{
    fm_assert(!is_passability_modified());
    return _hole_rtree.get();
}

const chunk::RTree* chunk::light_rtree() const noexcept
{
    fm_assert(!is_passability_modified());
    return _light_rtree.get();
}

namespace {

constexpr collision_data make_id_(collision_type type, pass_mode p, object_id id)
{
    fm_debug_assert(id < object_id{1} << collision_data_BITS);
    return collision_data { (object_id)type, (object_id)p, id };
}

constexpr object_id make_id(collision_type type, pass_mode p, object_id id)
{
    return std::bit_cast<object_id>(make_id_(type, p, id));
}

template<bool IsNeighbor>
bool add_holes_from_chunk(Chunk_RTree& rtree, chunk& c, Vector2b chunk_offset)
{
    bool has_holes = false;
    constexpr auto bounds = chunk_collision_bounds<Range2Di>;
    for (const struct hole& e : c.objects().list<struct hole>())
    {
        if (!e.flags.enabled)
            continue;
        if (Vector2ui{e.bbox_size}.product() == 0)
            continue;
        auto center = Vector2i(e.offset) + Vector2i(e.bbox_offset) + Vector2i(e.coord.local()) * iTILE_SIZE2;
        if constexpr(IsNeighbor)
        {
            const auto off = Vector2i(chunk_offset)*chunk_size<Vector2i>;
            center += off;
        }
        const auto min = center - Vector2i(e.bbox_size/2), max = min + Vector2i(e.bbox_size);
        if constexpr(IsNeighbor)
            if (!rect_intersects(min, max, bounds.min(), bounds.max())) [[likely]]
                continue;
        fm_assert((max > min).all());
        rtree.Insert(Vector2(min).data(), Vector2(max).data(), make_id(collision_type::none, e.pass, e.id));
        has_holes = true;
    }
    return has_holes;
}

// The covered part recurses with a threshold above the hole's, so the recursion ends.
void filter_bbox_through_holes(Chunk_RTree& rtree, const Chunk_RTree& holes, object_id id, Range2D bbox,
                               pass_mode min_pass, bool has_holes)
{
    fm_assert((bbox.max() > bbox.min()).all());
    if (!has_holes || min_pass > pass_mode::pass)
        return rtree.Insert(bbox.min().data(), bbox.max().data(), id);
    const auto x = std::bit_cast<collision_data>(id);
start:
    fm_assert((bbox.max() > bbox.min()).all());

    auto found = chunk::find_hole_in_bbox(holes, bbox, min_pass);

    if (!found) [[likely]]
        rtree.Insert(bbox.min().data(), bbox.max().data(), id);
    else
    {
        const auto [hole, hole_pass] = *found;
        auto res = CutResult<float>::cut(bbox, hole);
        if (!res.found())
        {
            rtree.Insert(bbox.min().data(), bbox.max().data(), id);
            return;
        }
        if (hole_pass != pass_mode::pass)
        {
            auto xʹ = x;
            xʹ.pass = (uint64_t)hole_pass;
            filter_bbox_through_holes(rtree, holes, std::bit_cast<object_id>(xʹ), Math::intersect(bbox, hole),
                                      pass_mode((unsigned)hole_pass + 1), has_holes);
        }
        if (res.size == 1)
        {
            bbox = res.array[0];
            goto start;
        }
        else
        {
            for (auto i = 0u; i < res.size; i++)
                filter_bbox_through_holes(rtree, holes, id, res.array[i], min_pass, has_holes);
        }
    }
}

} // namespace

Optional<Pair<Range2D, pass_mode>> chunk::find_hole_in_bbox(const Chunk_RTree& rtree, Range2D bbox, pass_mode min_pass)
{
    Optional<Pair<Range2D, pass_mode>> ret;
    rtree.Search(bbox.min().data(), bbox.max().data(), [&](uint64_t data, const Chunk_RTree::Rect& r) {
        auto x = std::bit_cast<collision_data>(data);
        if (x.pass >= (uint64_t)min_pass /*&& x.type == (uint64_t)collision_type::none*/)
        {
            Range2D hole {
                { r.m_min[0], r.m_min[1] },
                { r.m_max[0], r.m_max[1] },
            };
            if (rect_intersects(hole.min(), hole.max(), bbox.min(), bbox.max()))
            {
                ret = {InPlaceInit, hole, pass_mode(x.pass)};
                return false;
            }
        }
        return true;
    });
    return ret;
}
void chunk::get_all_holes_in_bbox(const hole_callback& fn, chunk& c, Vector2 bb_min, Vector2 bb_max,
                                  pass_through_mask mask)
{
    const auto& rtree = *c.hole_rtree();
    rtree.Search(bb_min.data(), bb_max.data(), [&](uint64_t data, const Chunk_RTree::Rect& r) {
        auto x = std::bit_cast<collision_data>(data);
        if (can_pass_through_mask(mask, pass_mode(x.pass)) /*&& x.type == (uint64_t)collision_type::none*/)
        {
            Vector2 hmin = {r.m_min[0], r.m_min[1]}, hmax = {r.m_max[0], r.m_max[1]};
            if (rect_intersects(hmin, hmax, bb_min, bb_max))
            {
                auto obj = c.world().find_object<struct hole>(x.id);
                fm_assert(obj);
                int zmaxʹ = (int)obj->z_offset + (int)obj->height;
                auto zmax = (uint8_t)Math::clamp(zmaxʹ, 0, tile_size_z);
                fn({hmin, hmax}, {obj->z_offset, zmax});
            }
        }
        return true;
    });
}

void chunk::ensure_passability() noexcept
{
    fm_assert(_objects._sorted); // not strictly necessary

    if (!_pass_modified)
        return;
    _pass_modified = false;

    _rtree->RemoveAll();
    _hole_rtree->RemoveAll();
    if (_light_rtree)
        _light_rtree->RemoveAll();

    //Debug{} << ".. reset passability" << _coord;

    bool has_holes = false;
    auto& rtree = *_rtree;
    auto& holes = *_hole_rtree;
    {
        has_holes |= add_holes_from_chunk<false>(holes, *this, {});
        const auto nbs = _world->neighbors(_coord);
        for (auto i = 0u; i < 8; i++)
            if (nbs[i])
                has_holes |= add_holes_from_chunk<true>(holes, *nbs[i], world::neighbor_offsets[i]);
    }

    for (auto i = 0u; i < TILE_COUNT; i++)
    {
        if (const auto* atlas = ground_atlas_at(i))
        {
            auto pass = atlas->pass_mode();
            if (pass == pass_mode::pass) [[likely]]
                continue;
            auto id = make_id(collision_type::geometry, pass, i+1);
            filter_bbox_through_holes(rtree, holes, id, whole_tile(i), pass_mode((unsigned)pass + 1), has_holes);
        }
    }
    for (auto i = 0u; i < TILE_COUNT; i++)
    {
        auto tile = operator[](i);
        if (const auto* atlas = tile.wall_north_atlas().get())
        {
            auto depth = (float)atlas->info().depth;
            auto pass = atlas->info().passability;
            auto id = make_id(collision_type::geometry, pass, TILE_COUNT+i+1);
            auto min_pass = pass_mode((unsigned)pass + 1);
            filter_bbox_through_holes(rtree, holes, id, wall_north(i, depth), min_pass, has_holes);

            if (const auto* west = tile.wall_west_atlas().get())
                filter_bbox_through_holes(rtree, holes, id, wall_pillar(i, (float)west->info().depth, depth),
                                          min_pass, has_holes);
        }
        if (const auto* atlas = tile.wall_west_atlas().get())
        {
            auto depth = (float)atlas->info().depth;
            auto pass = atlas->info().passability;
            auto id = make_id(collision_type::geometry, pass, TILE_COUNT*2+i+1);
            auto min_pass = pass_mode((unsigned)pass + 1);
            filter_bbox_through_holes(rtree, holes, id, wall_west(i, depth), min_pass, has_holes);
        }
    }
    for (const bptr<object>& eʹ : objects())
    {
        auto& e = *eʹ;
        if (e.type() == object_type::light)
        {
            if (bbox bb; _bbox_for_scenery(e, bb))
                _add_light_bbox(bb);
            continue;
        }
        if (e.updates_passability())
            continue;
        if (Vector2ui{e.bbox_size}.product() == 0)
            continue;
        bbox bb;
        if (_bbox_for_scenery(e, bb))
        {
            // a hole of equal mode cuts scenery too, so the editor can't pick a pass scenery under a pass hole
            if (!e.is_dynamic())
                filter_bbox_through_holes(rtree, holes, std::bit_cast<object_id>(bb.data),
                                          Math::Range2D<float>{bb.pos}, e.pass, has_holes);
            else
                _add_bbox_dynamic(bb);
        }
    }
    fm_assert(!_pass_modified);
}

uint64_t chunk::pass_gen() const noexcept { return _pass_gen; }

bool chunk::_bbox_for_scenery(const object& s, local_coords local, Vector2b offset,
                              Vector2b bbox_offset, Vector2ub bbox_size, bbox& value) noexcept
{
    auto [start, end] = scenery_tile(local, offset, bbox_offset, bbox_size);
    auto id = make_id_(collision_type::scenery, s.pass, s.id);
    value = { .data = id, .pos = { start, end } };
    return Vector2ui(bbox_size).product() > 0 && s.atlas;
}

bool chunk::_bbox_for_scenery(const object& s, bbox& value) noexcept
{
    return _bbox_for_scenery(s, s.coord.local(), s.offset, s.bbox_offset, s.bbox_size, value);
}

void chunk::_remove_bbox_static_(const bptr<object>& e)
{
    mark_passability_modified();
    e->mark_neighbor_chunks_modified();
}

void chunk::_add_bbox_static_(const bptr<object>& e)
{
    mark_passability_modified();
    e->mark_neighbor_chunks_modified();
}

void chunk::_rtree_remove(RTree& t, const bbox& x)
{
    auto start = Vector2(x.pos.min()), end = Vector2(x.pos.max());
    t.Remove(start.data(), end.data(), std::bit_cast<object_id>(x.data));
}

void chunk::_rtree_insert(RTree& t, const bbox& x)
{
    fm_assert((x.pos.max() > x.pos.min()).all());
    auto start = Vector2(x.pos.min()), end = Vector2(x.pos.max());
    t.Insert(start.data(), end.data(), std::bit_cast<object_id>(x.data));
}

void chunk::_add_light_bbox(const bbox& x)
{
    if (_pass_modified)
        return;
    if (!_light_rtree)
        _light_rtree = Pointer<RTree>{InPlaceInit};
    _rtree_insert(*_light_rtree, x);
}

void chunk::_remove_light_bbox(const bbox& x)
{
    if (_pass_modified)
        return;
    fm_assert(_light_rtree);
    _rtree_remove(*_light_rtree, x);
}

void chunk::_replace_light_bbox(const bbox& x0, const bbox& x, bool b0, bool b)
{
    if (b0 && b && x0 == x)
        return;
    if (b0)
        _remove_light_bbox(x0);
    if (b)
        _add_light_bbox(x);
}

void chunk::_remove_bbox_dynamic(const bbox& x)
{
    _rtree_remove(*_rtree, x);
    //Debug{} << "bbox <<< dynamic" << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

void chunk::_remove_bbox_static(const bptr<object>& e, [[maybe_unused]] const bbox& x)
{
    _remove_bbox_static_(e);
    //Debug{} << "bbox <<< static " << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

void chunk::_add_bbox_dynamic(const bbox& x)
{
    _rtree_insert(*_rtree, x);
    //Debug{} << "bbox >>> dynamic" << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

void chunk::_add_bbox_static(const bptr<object>& e, [[maybe_unused]]const bbox& x)
{
    _add_bbox_static_(e);
    //Debug{} << "bbox >>> static " << x.data.pass << x.data.data << x.start << x.end << _rtree->Count();
}

template<bool Dynamic>
void chunk::_replace_bbox_impl(const bptr<object>& e, const bbox& x0, const bbox& x1, bool b0, bool b1)
{
    // the static branch only sets dirty flags, and one of them lands on the neighbor
    // chunks, which _pass_modified doesn't cover. Same reasoning as add_object_pre().
    if constexpr(Dynamic)
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
    fm_assert(false);
}

void chunk::_replace_bbox_dynamic(const bbox& x0, const bbox& x, bool b0, bool b)
{
    _replace_bbox_impl<true>(nullptr, x0, x, b0, b);
}

void chunk::_replace_bbox_static(const bptr<object>& e, const bbox& x0, const bbox& x, bool b0, bool b)
{
    _replace_bbox_impl<false>(e, x0, x, b0, b);
}

void chunk::_replace_bbox_(const bptr<object>& e, const bbox& x0, const bbox& x,
                           bool b0, bool b, bool upd, bool is_dynamic)
{
    if (e->type() == object_type::light)
        _replace_light_bbox(x0, x, b0, b);
    else if (!is_dynamic || upd)
        _replace_bbox_static(e, x0, x, b0, b);
    else
        _replace_bbox_dynamic(x0, x, b0, b);
}

bool chunk::can_place_object(const object_proto& proto, local_coords pos)
{
    fm_assert(proto.pass < pass_mode::COUNT);
    switch (proto.pass)
    {
    case pass_mode::COUNT: std::unreachable();
    case pass_mode::blocked:
    case pass_mode::see_through:
    case pass_mode::shoot_through:
        break;
    case pass_mode::pass:
        return true;
    }

    const auto bbox_size = Vector2(proto.bbox_size);
    const auto center = Vector2(pos)*TILE_SIZE2 + Vector2(proto.offset) + Vector2(proto.bbox_offset);
    const auto min = center - Vector2(proto.bbox_size/2), max = min + bbox_size;
    return Search::is_passable_(this, _world->neighbors(_coord), min, max, constantly(path_search_continue::blocked));
}

} // namespace floormat
