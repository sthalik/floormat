#include "chunk.hpp"
#include "object.hpp"
#include "world.hpp"
#include "tile-iterator.hpp"
#include "RTree.h"
#include <algorithm>
#include <Corrade/Containers/GrowableArray.h>
#include <Magnum/GL/Context.h>

namespace floormat {

namespace {

constexpr auto object_id_lessp = [](const auto& a, const auto& b) { return a->id < b->id; };

size_t _reload_no_ = 0; // NOLINT

bool is_log_quiet()
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::QuietLog);
}

bool is_log_verbose()
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !!(flags & GLCCF::VerboseLog);
}

bool is_log_standard()
{
    using GLCCF = GL::Implementation::ContextConfigurationFlag;
    auto flags = GL::Context::current().configurationFlags();
    return !(flags & (GLCCF::VerboseLog|GLCCF::QuietLog));
}

} // namespace

bool chunk::empty(bool force) const noexcept
{
    if (!force && !_maybe_empty) [[likely]]
        return false;
    for (auto i = 0uz; i < TILE_COUNT; i++)
        if (!_objects.isEmpty() ||
            _ground && _ground->atlases[i] ||
            _walls && (_walls->atlases[i*2+0] || _walls->atlases[i*2+1]))
            return _maybe_empty = false;
    if (!_objects.isEmpty())
        return false;
    return true;
}

ground_atlas* chunk::ground_atlas_at(size_t i) const noexcept { return _ground ? _ground->atlases[i].get() : nullptr; }

tile_ref chunk::operator[](size_t idx) noexcept { return { *this, uint8_t(idx) }; }
tile_proto chunk::operator[](size_t idx) const noexcept { return tile_proto(tile_ref { *const_cast<chunk*>(this), uint8_t(idx) }); }
tile_ref chunk::operator[](local_coords xy) noexcept { return operator[](xy.to_index()); }
tile_proto chunk::operator[](local_coords xy) const noexcept { return operator[](xy.to_index()); }

chunk_coords_ chunk::coord() const noexcept { return _coord; }

tile_ref chunk::at_offset(local_coords pos, Vector2i off)
{
    const auto coord = global_coords{_coord, pos};
    const auto coord2 = coord + off;
    if (coord.chunk() == coord2.chunk()) [[likely]]
        return operator[](coord2.local());
    else
        return (*_world)[coord2].t;
}

Optional<tile_ref> chunk::at_offset_(local_coords pos, Vector2i off)
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

tile_ref chunk::at_offset(tile_ref r, Vector2i off) { return at_offset(local_coords{r.index()}, off); }
Optional<tile_ref> chunk::at_offset_(tile_ref r, Vector2i off) { return at_offset_(local_coords{r.index()}, off); }

auto chunk::begin() noexcept -> iterator { return iterator { *this, 0 }; }
auto chunk::end() noexcept -> iterator { return iterator { *this, TILE_COUNT }; }
auto chunk::cbegin() const noexcept -> const_iterator { return const_iterator { *this, 0 }; }
auto chunk::cend() const noexcept -> const_iterator { return const_iterator { *this, TILE_COUNT }; }
auto chunk::begin() const noexcept -> const_iterator { return cbegin(); }
auto chunk::end() const noexcept -> const_iterator { return cend(); }

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
        fm_debug("pass reload %zu", ++_reload_no_);
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

bool chunk::bbox::operator==(const bbox& other) const noexcept = default;

void chunk::add_object_unsorted(const std::shared_ptr<object>& e)
{
    _objects_sorted = false;
    if (!e->is_dynamic())
        mark_scenery_modified();
    if (bbox bb; _bbox_for_scenery(*e, bb))
        _add_bbox(bb);
    arrayAppend(_objects, e);
}

void chunk::sort_objects()
{
    if (_objects_sorted)
        return;

    _objects_sorted = true;
    mark_scenery_modified();

    std::sort(_objects.begin(), _objects.end(), [](const auto& a, const auto& b) {
        return a->id < b->id;
    });
}

void chunk::add_object(const std::shared_ptr<object>& e)
{
    fm_assert(_objects_sorted);
    if (!e->is_dynamic())
        mark_scenery_modified();
    if (bbox bb; _bbox_for_scenery(*e, bb))
        _add_bbox(bb);
    auto& es = _objects;
    auto it = std::lower_bound(es.cbegin(), es.cend(), e, object_id_lessp);
    arrayInsert(es, (size_t)std::distance(es.cbegin(), it), e);
}

void chunk::remove_object(size_t i)
{
    fm_assert(_objects_sorted);
    auto& es = _objects;
    fm_debug_assert(i < es.size());
    const auto e = es[i];
    if (!e->is_dynamic())
        mark_scenery_modified();
    if (bbox bb; _bbox_for_scenery(*e, bb))
        _remove_bbox(bb);
    arrayRemove(es, i);
}

ArrayView<const std::shared_ptr<object>> chunk::objects() const
{
    fm_assert(_objects_sorted);
    return _objects;
}

} // namespace floormat
