#include "chunk.hpp"
#include "object.hpp"
#include "world.hpp"
#include "log.hpp"
#include "RTree.h"
#include "compat/non-const.hpp"
#include "ground-atlas.hpp"
#include <cr/Optional.h>

namespace floormat {

namespace {

size_t _reload_no_ = 0; // NOLINT

} // namespace

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty) [[likely]]
        return false;
    if (!_objects.empty())
        return _maybe_empty = false;
    for (auto i = 0uz; i < TILE_COUNT; i++)
        if (_ground && _ground->atlases[i] ||
            _walls && (_walls->atlases[i*2+0] || _walls->atlases[i*2+1]))
            return _maybe_empty = false;
    return true;
}

ground_atlas* chunk::ground_atlas_at(size_t i) const noexcept { return _ground ? _ground->atlases[i].get() : nullptr; }

tile_ref chunk::operator[](size_t idx) noexcept { return { *this, uint8_t(idx) }; }
const_tile_ref chunk::operator[](size_t idx) const noexcept { return { *this, uint8_t(idx) }; }
tile_ref chunk::operator[](local_coords xy) noexcept { return operator[](xy.to_index()); }
const_tile_ref chunk::operator[](local_coords xy) const noexcept { return operator[](xy.to_index()); }

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

Optional<const_tile_ref> chunk::at_offset(local_coords pos, Vector2i off) const
{
    if (auto r = non_const(*this).at_offset(pos, off))
        return const_tile_ref{r->chunk(), uint8_t(r->index())};
    return NullOpt;
}

Optional<const_tile_ref> chunk::at_offset(const_tile_ref r, Vector2i off) const { return at_offset(local_coords{r.index()}, off); }

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
    _pass_gen = _world->next_pass_gen();
}

bool chunk::is_passability_modified() const noexcept { return _pass_modified; }
bool chunk::is_scenery_modified() const noexcept { return _scenery_modified; }
bool chunk::are_walls_modified() const noexcept { return _walls_modified; }

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
    _coord{ch},
    _pass_gen{w.next_pass_gen()}
{
    _world->register_chunk(this);
}

chunk::~chunk() noexcept
{
    _world->unregister_chunk(this);
    _teardown = true;
    _objects.clear();
    _rtree->RemoveAll();
}

void chunk::sort_objects()
{
    if (_objects.sort())
        mark_scenery_modified();
}

void chunk::add_object_pre(const bptr<object>& e)
{
    fm_assert(&*e->c == this);
    const bool dyn = e->is_dynamic();
    const bool upd_passability = e->updates_passability();
    const bool upd_walls = e->updates_walls();
    if (!dyn)
        mark_scenery_modified();
    // the static path isn't gated on _pass_modified because it only sets dirty flags,
    // and one of them lands on the neighbor chunks
    if (!dyn || upd_passability)
        _add_bbox_static_(e);
    else if (!_pass_modified) [[likely]]
    {
        if (bbox bb; _bbox_for_scenery(*e, bb))
            _add_bbox_dynamic(bb);
    }
    if (upd_walls)
        mark_walls_modified();
}

void chunk::add_object_unsorted(const bptr<object>& e)
{
    add_object_pre(e);
    _objects.append(e);
}

size_t chunk::add_objectʹ(const bptr<object>& e)
{
    add_object_pre(e);
    return _objects.insert(e);
}

void chunk::add_object(const bptr<object>& e) { (void)add_objectʹ(e); }

void chunk::on_teardown() // NOLINT(*-make-member-function-const)
{
    fm_assert(!_teardown); // too late, some chunks were already erased
}

bool chunk::is_teardown() const { return _teardown || _world->is_teardown(); }

void chunk::remove_object(const object& e, size_t i)
{
    const auto eʹ = _objects.ptr(e, i);
    fm_assert(e.c == this);

    const bool dyn = e.is_dynamic();
    const bool upd_passability = e.updates_passability();
    const bool upd_walls = e.updates_walls();
    if (!dyn)
        mark_scenery_modified();

    if (!dyn || upd_passability)
        _remove_bbox_static_(eʹ);
    else if (!_pass_modified) [[likely]]
    {
        if (bbox bb; _bbox_for_scenery(e, bb))
            _remove_bbox_dynamic(bb);
    }

    if (upd_walls)
        mark_walls_modified();

    _objects.erase(e, i);
}

void chunk::kill_object(const object& e, size_t i, script_destroy_reason r)
{
    auto eʹ = _objects.ptr(e, i);
    // the script is handed a live bptr, so the object can only be deleted after it's torn down
    eʹ->destroy_script_pre(eʹ, r);
    remove_object(e, i);
    eʹ->destroy_script_post();
    eʹ.destroy();
}

const object_storage& chunk::objects() const { return _objects; }
object_storage& chunk::objects() { return _objects; }

} // namespace floormat
