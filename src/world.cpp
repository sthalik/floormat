#include "world.hpp"
#include "chunk.hpp"
#include "object.hpp"
#include "compat/int-hash.hpp"
#include "compat/exception.hpp"

using namespace floormat;

size_t world::object_id_hasher::operator()(object_id id) const noexcept { return (size_t)int_hash(id); }

size_t world::chunk_coords_hasher::operator()(const chunk_coords_& coord) const noexcept
{
    uint64_t x = 0;
    x |= uint64_t((uint16_t)coord.x) << 0;
    x |= uint64_t((uint16_t)coord.y) << 16;
    x |= uint64_t( (uint8_t)coord.z) << 32;
    return (size_t)int_hash(x);
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
        _objects = std::move(w._objects);
        _object_counter = w._object_counter;
        _current_frame = w._current_frame;
        w._object_counter = 0;

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
        v._objects.clear();
    }
    _last_chunk = {};
    _chunks.clear();
    _objects.clear();
}

world::world(size_t capacity) : _chunks{capacity}
{
    _chunks.max_load_factor(max_load_factor);
    _chunks.reserve(initial_capacity);
    _objects.max_load_factor(max_load_factor);
    _objects.reserve(initial_capacity);
}

chunk& world::operator[](chunk_coords_ coord) noexcept
{
    fm_debug_assert(coord.z >= chunk_z_min && coord.z <= chunk_z_max);
    auto& [c, coord2] = _last_chunk;
    if (coord != coord2)
        c = &_chunks.try_emplace(coord, *this, coord).first->second;
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
    _objects.clear();
    _objects.rehash(initial_capacity);
    _collect_every = initial_collect_every;
    _object_counter = object_counter_init;
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

void world::do_make_object(const std::shared_ptr<object>& e, global_coords pos, bool sorted)
{
    fm_assert(e->id > 0);
    fm_debug_assert(_unique_id && e->c->world()._unique_id == _unique_id);
    fm_assert(!_objects.contains(e->id));
    fm_assert(e->type() != object_type::none);
    const_cast<global_coords&>(e->coord) = pos;
    _objects[e->id] = e;
    if (sorted)
        e->c->add_object(e);
    else
        e->c->add_object_unsorted(e);
}

void world::do_kill_object(object_id id)
{
    fm_debug_assert(id > 0);
    auto cnt = _objects.erase(id);
    fm_debug_assert(cnt > 0);
}

std::shared_ptr<object> world::find_object_(object_id id)
{
    auto it = _objects.find(id);
    auto ret = it == _objects.end() ? nullptr : it->second.lock();
    fm_debug_assert(!ret || &ret->c->world() == this);
    return ret;
}

void world::set_object_counter(object_id value)
{
    fm_assert(value >= _object_counter);
    _object_counter = value;
}

void world::throw_on_wrong_object_type(object_id id, object_type actual, object_type expected)
{
    fm_throw("object '{}' has wrong object type '{}', should be '{}'"_cf, id, (size_t)actual, (size_t)expected);
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
