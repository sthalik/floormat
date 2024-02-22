#include "world.hpp"
#include "chunk.hpp"
#include "object.hpp"
#include "compat/int-hash.hpp"
#include "compat/exception.hpp"
#include <Corrade/Containers/GrowableArray.h>
#include <tsl/robin_map.h>

using namespace floormat;

size_t world::object_id_hasher::operator()(object_id id) const noexcept { return hash_int(id); }

size_t world::chunk_coords_hasher::operator()(const chunk_coords_& coord) const noexcept
{
    uint64_t x = 0;
    x |= uint64_t((uint16_t)coord.x) << 0;
    x |= uint64_t((uint16_t)coord.y) << 16;
    x |= uint64_t( (uint8_t)coord.z) << 32;
    return hash_int(x);
}

namespace floormat {

struct world::robin_map_wrapper final : tsl::robin_map<object_id, std::weak_ptr<object>, object_id_hasher>
{
    using tsl::robin_map<object_id, std::weak_ptr<object>, object_id_hasher>::robin_map;
};

world::world(world&& w) noexcept = default;

world::world(std::unordered_map<chunk_coords_, chunk>&& chunks) :
    world{std::max(initial_capacity, size_t(1/max_load_factor * 2 * chunks.size()))}
{
    for (auto&& [coord, c] : chunks)
        operator[](coord) = std::move(c);
}

world& world::operator=(world&& w) noexcept
{
    fm_debug_assert(&w != this);
    fm_assert(!w._teardown);
    fm_assert(!_teardown);
    fm_assert(w._unique_id);
    _last_chunk = {};
    _chunks = std::move(w._chunks);
    _objects = std::move(w._objects);
    w._objects = {};
    _unique_id = std::move(w._unique_id);
    fm_debug_assert(_unique_id);
    fm_debug_assert(w._unique_id == nullptr);
    _object_counter = w._object_counter;
    w._object_counter = 0;
    _current_frame = w._current_frame;

    for (auto& [id, c] : _chunks)
        c._world = this;
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
        arrayResize(v._objects, 0);
    }
    _last_chunk = {};
    _chunks.clear();
    _objects->clear();
}

world::world(size_t capacity) : _chunks{capacity}
{
    _chunks.max_load_factor(max_load_factor);
    _chunks.reserve(initial_capacity);
    _objects->max_load_factor(max_load_factor);
    _objects->reserve(initial_capacity);
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
    auto& c = operator[](pt.chunk3());
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
    _chunks.clear();
    _chunks.rehash(initial_capacity);
    _objects->clear();
    _objects->rehash(initial_capacity);
    _object_counter = object_counter_init;
    auto& [c, pos] = _last_chunk;
    c = nullptr;
    pos = chunk_tuple::invalid_coords;
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
    fm_assert(!_objects->contains(e->id));
    fm_assert(e->type() != object_type::none);
    const_cast<global_coords&>(e->coord) = pos;
    (*_objects)[e->id] = e;
    if (sorted)
        e->c->add_object(e);
    else
        e->c->add_object_unsorted(e);
}

void world::do_kill_object(object_id id)
{
    fm_debug_assert(id > 0);
    auto cnt = _objects->erase(id);
    fm_debug_assert(cnt > 0);
}

std::shared_ptr<object> world::find_object_(object_id id)
{
    auto it = _objects->find(id);
    auto ret = it == _objects->end() ? nullptr : it->second.lock();
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

auto world::neighbors(chunk_coords_ coord) -> std::array<chunk*, 8>
{
    std::array<chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = at(coord + neighbor_offsets[i]);
    return ret;
}

} // namespace floormat
