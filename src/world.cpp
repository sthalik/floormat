#include "world.hpp"
#include "chunk.hpp"
#include "entity.hpp"

namespace floormat {

world::world(world&& w) noexcept = default;

world& world::operator=(world&& w) noexcept
{
    fm_assert(!w._teardown);
    if (&w != this) [[likely]]
    {
        _last_collection = w._last_collection;
        _collect_every = w._collect_every;
        _unique_id = std::move(w._unique_id);
        w._unique_id = std::make_shared<char>('D');
        _last_chunk = {};
        _chunks = std::move(w._chunks);
        _entities = std::move(w._entities);

        for (auto& [id, c] : _chunks)
            c._world = this;
    }
    return *this;
}

world::world() : world{initial_capacity}
{
}

world::~world() noexcept
{
    _teardown = true;
    for (auto& [k, v] : _chunks)
    {
        v._teardown = true;
        v.mark_scenery_modified(true);
        _last_chunk = {};
        v._entities.clear();
    }
    _last_chunk = {};
    _chunks.clear();
    _entities.clear();
}

world::world(std::size_t capacity) : _chunks{capacity, hasher}
{
    _chunks.max_load_factor(max_load_factor);
}

chunk& world::operator[](chunk_coords coord) noexcept
{
    auto& [c, coord2] = _last_chunk;
    if (coord != coord2)
        c = &_chunks.try_emplace(coord, *this).first->second;
    coord2 = coord;
    return *c;
}

auto world::operator[](global_coords pt) noexcept -> pair
{
    auto& c = operator[](pt.chunk());
    return { c, c[pt.local()] };
}

bool world::contains(chunk_coords c) const noexcept
{
    return _chunks.find(c) != _chunks.cend();
}

void world::clear()
{
    _last_collection = 0;
    _chunks.clear();
    _chunks.rehash(initial_capacity);
    auto& [c, pos] = _last_chunk;
    c = nullptr;
    pos = chunk_tuple::invalid_coords;
}

void world::maybe_collect()
{
    if (_chunks.size() > _last_collection + _collect_every)
        collect();
}

void world::collect(bool force)
{
    const auto len0 = _chunks.size();
    for (auto it = _chunks.begin(); it != _chunks.end(); (void)0)
    {
        const auto& [_, c] = *it;
        if (c.empty(force))
            it = _chunks.erase(it);
        else
            ++it;
    }

    _last_collection = _chunks.size();
    auto& [c, pos] = _last_chunk;
    c = nullptr;
    pos = chunk_tuple::invalid_coords;
    const auto len = len0 - _chunks.size();
    if (len)
        fm_debug("world: collected %zu/%zu chunks", len, len0);
}

void world::do_make_entity(const std::shared_ptr<entity>& e, global_coords pos, bool sorted)
{
    fm_assert(e->id > 0);
    fm_debug_assert(e->c->world()._unique_id == _unique_id);
    fm_assert(!_entities.contains(e->id));
    fm_assert(Vector2ui(e->bbox_size).product() > 0);
    fm_assert(e->type != entity_type::none);
    e->coord = pos;
    _entities[e->id] = e;
    if (sorted)
        e->c->add_entity(e);
    else
        e->c->add_entity_unsorted(e);
}

void world::do_kill_entity(std::uint64_t id)
{
    fm_debug_assert(id > 0);
    auto cnt = _entities.erase(id);
    fm_debug_assert(cnt > 0);
}

std::shared_ptr<entity> world::find_entity_(std::uint64_t id)
{
    auto it = _entities.find(id);
    auto ret = it == _entities.end() ? nullptr : it->second.lock();
    fm_debug_assert(!ret || &ret->c->world() == this);
    return ret;
}

void world::set_entity_counter(std::uint64_t value)
{
    fm_assert(value >= _entity_counter);
    _entity_counter = value;
}

} // namespace floormat
