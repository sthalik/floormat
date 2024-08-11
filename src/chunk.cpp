#include "chunk.hpp"
#include "object.hpp"
#include "world.hpp"
#include "log.hpp"
#include "RTree.h"
#include "compat/non-const.hpp"
#include "ground-atlas.hpp"
#include <algorithm>
#include <cr/GrowableArray.h>
#include <cr/Optional.h>

namespace floormat {

namespace {

constexpr auto object_id_lessp = [](const auto& a, const auto& b) { return a->id < b->id; };

size_t _reload_no_ = 0; // NOLINT

} // namespace

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty) [[likely]]
        return false;
    if (!_objects.isEmpty())
        return _maybe_empty = false;
    for (auto i = 0uz; i < TILE_COUNT; i++)
        if (_ground && _ground->atlases[i] ||
            _walls && (_walls->atlases[i*2+0] || _walls->atlases[i*2+1]))
            return _maybe_empty = false;
    return true;
}

ground_atlas* chunk::ground_atlas_at(size_t i) const noexcept { return _ground ? _ground->atlases[i].get() : nullptr; }

tile_ref chunk::operator[](size_t idx) noexcept { return { *this, uint8_t(idx) }; }
tile_proto chunk::operator[](size_t idx) const noexcept { return tile_proto(tile_ref { non_const(*this), uint8_t(idx) }); }
tile_ref chunk::operator[](local_coords xy) noexcept { return operator[](xy.to_index()); }
tile_proto chunk::operator[](local_coords xy) const noexcept { return operator[](xy.to_index()); }

chunk_coords_ chunk::coord() const noexcept { return _coord; }

Optional<tile_ref> chunk::at_offset(local_coords pos, Vector2i off)
{
    const auto coord = global_coords{_coord, pos};
    const auto coord2 = coord + off;
    if (coord.chunk() == coord2.chunk()) [[likely]]
        return operator[](coord2.local());
    else
    {
        if (auto* ch = _world->at(coord2.chunk3()))
            return (*ch)[coord2.local()];
        else
            return NullOpt;
    }
}

Optional<tile_ref> chunk::at_offset(tile_ref r, Vector2i off) { return at_offset(local_coords{r.index()}, off); }

void chunk::mark_ground_modified() noexcept
{
    if (!_ground_modified && is_log_verbose()) [[unlikely]]
        fm_debug("ground reload %zu", ++_reload_no_);
    _ground_modified = true;
    mark_passability_modified();
}

void chunk::mark_walls_modified() noexcept
{
    if (!_walls_modified && is_log_verbose()) [[unlikely]]
        fm_debug("wall reload %zu", ++_reload_no_);
    _walls_modified = true;
    mark_passability_modified();
}

void chunk::mark_scenery_modified() noexcept
{
    if (!_scenery_modified && is_log_verbose()) [[unlikely]]
        fm_debug("scenery reload %zu", ++_reload_no_);
    _scenery_modified = true;
}

void chunk::mark_passability_modified() noexcept
{
    if (!_pass_modified && is_log_verbose()) [[unlikely]]
        fm_debug("pass reload %zu (%d:%d:%d)", ++_reload_no_, int{_coord.x}, int{_coord.y}, int{_coord.z});
    _pass_modified = true;
}

bool chunk::is_passability_modified() const noexcept { return _pass_modified; }
bool chunk::is_scenery_modified() const noexcept { return _scenery_modified; }

void chunk::mark_modified() noexcept
{
    mark_ground_modified();
    mark_walls_modified();
    mark_scenery_modified();
    mark_passability_modified();
}

chunk::chunk(class world& w, chunk_coords_ ch) noexcept :
    _world{&w},
    _rtree{InPlaceInit},
    _coord{ch}
{
}

chunk::~chunk() noexcept
{
    _teardown = true;
    arrayResize(_objects, 0);
    arrayShrink(_objects);
    _rtree->RemoveAll();
}

chunk::chunk(chunk&&) noexcept = default;
chunk& chunk::operator=(chunk&&) noexcept = default;

void chunk::sort_objects()
{
    if (_objects_sorted)
        return;
    _objects_sorted = true;
    mark_scenery_modified();
    std::sort(_objects.begin(), _objects.end(), object_id_lessp);
}

void chunk::add_object_pre(const bptr<object>& e)
{
    fm_assert(&*e->c == this);
    const auto dyn = e->is_dynamic(), upd = e->updates_passability();
    if (!dyn)
        mark_scenery_modified();
    if (!_pass_modified) [[likely]]
    {
        if (!dyn || upd)
            _add_bbox_static_(e);
        else if (bbox bb; _bbox_for_scenery(*e, bb))
            _add_bbox_dynamic(bb);
    }
}

void chunk::add_object_unsorted(const bptr<object>& e)
{
    add_object_pre(e);
    _objects_sorted = false;
    arrayReserve(_objects, 8);
    arrayAppend(_objects, e);
}

size_t chunk::add_objectʹ(const bptr<object>& e)
{
    fm_assert(_objects_sorted);
    add_object_pre(e);
    auto& es = _objects;
    arrayReserve(es, 8);
    auto* it = std::lower_bound(es.data(), es.data() + es.size(), e, object_id_lessp);
    auto i = (size_t)std::distance(es.data(), it);
    arrayInsert(es, i, e);
    return i;
}

void chunk::add_object(const bptr<object>& e) { (void)add_objectʹ(e); }

void chunk::on_teardown() // NOLINT(*-make-member-function-const)
{
    fm_assert(!_teardown); // too late, some chunks were already erased
}

bool chunk::is_teardown() const { return _teardown || _world->is_teardown(); }

void chunk::remove_object(size_t i)
{
    fm_assert(_objects_sorted);
    fm_debug_assert(i < _objects.size());

    auto eʹ = _objects[i];
    {
        auto& e = *eʹ;
        fm_assert(e.c == this);

        const auto dyn = e.is_dynamic(), upd = e.updates_passability();
        if (!dyn)
            mark_scenery_modified();

        if (!_pass_modified) [[likely]]
        {
            if (!dyn || upd)
                _remove_bbox_static_(eʹ);
            else if (bbox bb; _bbox_for_scenery(e, bb))
                _remove_bbox_dynamic(bb);
        }

    }
    arrayRemove(_objects, i);
}

ArrayView<const bptr<object>> chunk::objects() const
{
    fm_assert(_objects_sorted);
    return _objects;
}

} // namespace floormat
