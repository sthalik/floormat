#include "world.hpp"
#include "chunk.hpp"
#include "entity.hpp"

namespace floormat {

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
        c = &_chunks.try_emplace(coord).first->second;
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

static constexpr std::uint64_t min_id = 1u << 16;
std::uint64_t world::entity_counter = min_id;

void world::do_make_entity(const std::shared_ptr<entity>& e, global_coords pos)
{
    fm_debug_assert(e->id > min_id && &e->w == this);
    fm_assert(Vector2ui(e->bbox_size).product() > 0);
    fm_assert(e->type != entity_type::none);
    e->coord = pos;
    _entities[e->id] = e;
    operator[](pos.chunk()).add_entity(e);
}

void world::do_kill_entity(std::uint64_t id)
{
    fm_debug_assert(id > min_id);
    auto cnt = _entities.erase(id);
    fm_debug_assert(cnt > 0);
}

} // namespace floormat
