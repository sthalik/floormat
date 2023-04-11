#include "world.hpp"
#include "chunk.hpp"
#include "entity.hpp"
#include "compat/int-hash.hpp"
#include "compat/exception.hpp"

using namespace floormat;

size_t std::hash<chunk_coords_>::operator()(const chunk_coords_& coord) const noexcept
{
    std::size_t x = 0;

    x |= size_t(uint16_t(coord.y)) << 16;
    x |= size_t(uint16_t(coord.x));
    if constexpr(sizeof(size_t) > 4)
        x |= size_t(uint8_t(coord.z- chunk_z_min) & 0xf) << 32;
    else
        x ^= size_t(uint8_t(coord.z- chunk_z_min) & 0xf) * size_t(1664525);

    if constexpr(sizeof(size_t) > 4)
        return int_hash(uint64_t(x));
    else
        return int_hash(uint32_t(x));
}

namespace floormat {

world::world(world&& w) noexcept = default;

world::world(std::unordered_map<chunk_coords_, chunk>&& chunks) :
    world{std::max(initial_capacity, size_t(1/max_load_factor * 2 * chunks.size()))}
{
    for (auto&& [coord, c] : chunks)
        operator[](coord) = std::move(c);
}

world& world::operator=(world&& w) noexcept
{
    if (&w != this) [[likely]]
    {
        fm_assert(!w._teardown);
        fm_assert(!_teardown);
        _last_collection = w._last_collection;
        _collect_every = w._collect_every;
        _unique_id = std::move(w._unique_id);
        fm_assert(_unique_id);
        fm_debug_assert(w._unique_id == nullptr);
        _last_chunk = {};
        _chunks = std::move(w._chunks);
        _entities = std::move(w._entities);
        _entity_counter = w._entity_counter;
        _current_frame = w._current_frame;
        w._entity_counter = 0;

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
        v.mark_scenery_modified();
        v.mark_passability_modified();
        _last_chunk = {};
        v._entities.clear();
    }
    _last_chunk = {};
    _chunks.clear();
    _entities.clear();
}

world::world(size_t capacity) : _chunks{capacity}
{
    _chunks.max_load_factor(max_load_factor);
    _chunks.reserve(initial_capacity);
    _entities.max_load_factor(max_load_factor);
    _entities.reserve(initial_capacity);
}

chunk& world::operator[](chunk_coords_ coord) noexcept
{
    fm_debug_assert(coord.z >= chunk_z_min && coord.z <= chunk_z_max);
    auto& [c, coord2] = _last_chunk;
    if (coord != coord2)
        c = &_chunks.try_emplace(coord, *this).first->second;
    coord2 = coord;
    return *c;
}

auto world::operator[](global_coords pt) noexcept -> pair
{
    auto& c = operator[]({pt.chunk(), pt.z()});
    return { c, c[pt.local()] };
}

chunk* world::at(chunk_coords_ c) noexcept
{
    auto it = _chunks.find(c);
    if (it != _chunks.end())
        return &it->second;
    else
        return nullptr;
}

bool world::contains(chunk_coords_ c) const noexcept
{
    return _chunks.find(c) != _chunks.cend();
}

void world::clear()
{
    fm_assert(!_teardown);
    _last_collection = 0;
    _chunks.clear();
    _chunks.rehash(initial_capacity);
    _entities.clear();
    _entities.rehash(initial_capacity);
    _collect_every = initial_collect_every;
    _entity_counter = entity_counter_init;
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
    fm_debug_assert(_unique_id && e->c->world()._unique_id == _unique_id);
    fm_assert(!_entities.contains(e->id));
    fm_assert(Vector2ui(e->bbox_size).product() > 0);
    fm_assert(e->type() != entity_type::none);
    const_cast<global_coords&>(e->coord) = pos;
    _entities[e->id] = e;
    if (sorted)
        e->c->add_entity(e);
    else
        e->c->add_entity_unsorted(e);
}

void world::do_kill_entity(object_id id)
{
    fm_debug_assert(id > 0);
    auto cnt = _entities.erase(id);
    fm_debug_assert(cnt > 0);
}

std::shared_ptr<entity> world::find_entity_(object_id id)
{
    auto it = _entities.find(id);
    auto ret = it == _entities.end() ? nullptr : it->second.lock();
    fm_debug_assert(!ret || &ret->c->world() == this);
    return ret;
}

void world::set_entity_counter(object_id value)
{
    fm_assert(value >= _entity_counter);
    _entity_counter = value;
}

void world::throw_on_wrong_entity_type(object_id id, entity_type actual, entity_type expected)
{
    fm_throw("object '{}' has wrong entity type '{}', should be '{}'"_cf, id, (size_t)actual, (size_t)expected);
}

auto world::neighbors(floormat::chunk_coords_ coord) -> std::array<neighbor_pair, 8>
{
    std::array<neighbor_pair, 8> ret;
    for (auto i = 0uz; const auto& x : neighbor_offsets)
    {
        auto ch = coord + x;
        ret[i++] = { at(ch), ch };
    }
    return ret;
}

} // namespace floormat
