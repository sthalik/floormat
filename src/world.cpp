#include "world.hpp"
#include "chunk.hpp"
#include "object.hpp"
#include "critter.hpp"
#include "scenery.hpp"
#include "scenery-proto.hpp"
#include "light.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/weak-borrowed-ptr.inl"
#include "compat/hash.hpp"
#include "compat/exception.hpp"
#include "compat/overloaded.hpp"
#include <cr/GrowableArray.h>
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
struct world::robin_map_wrapper final : tsl::robin_map<object_id, weak_bptr<object>, object_id_hasher>
{
    using tsl::robin_map<object_id, weak_bptr<object>, object_id_hasher>::robin_map;
};

world::world(world&& w) noexcept = default;

world::world(std::unordered_map<chunk_coords_, chunk>&& chunks) :
    world{std::max(initial_capacity, size_t(1/max_load_factor * 2 * chunks.size()))}
{
    for (auto&& [coord, c] : chunks)
        operator[](coord) = move(c);
}

world& world::operator=(world&& w) noexcept
{
    fm_debug_assert(&w != this);
    fm_assert(!w._script_initialized);
    fm_assert(!w._script_finalized);
    fm_assert(!_script_initialized || _script_finalized);
    _script_initialized = false;
    _script_finalized = false;
    fm_assert(!w._teardown);
    fm_assert(!_teardown);
    _last_chunk = {};
    _chunks = move(w._chunks);
    _objects = move(w._objects);
    w._objects = {};
    fm_assert(w._unique_id);
    _unique_id = move(w._unique_id);
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
    fm_assert(_script_finalized || !_script_initialized);
    for (auto& [k, c] : _chunks)
        c.on_teardown();
    _teardown = true;
    _objects->clear();
    for (auto& [k, c] : _chunks)
    {
        c._teardown = true;
        //c.mark_scenery_modified(); // XXX why? what for?
        //c.mark_passability_modified(); // XXX why? what for?
        _last_chunk = {};
        arrayResize(c._objects, 0);
    }
    _last_chunk = {};
    _chunks.clear();
}

bool world::unique_id::operator==(const unique_id& other) const { return this == &other; }

world::world(size_t capacity) : _chunks{capacity}, _unique_id{InPlace}
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

auto world::operator[](global_coords pt) noexcept -> pair_chunk_tile
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
    return _chunks.contains(c);
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
    if (len > 1)
        fm_debug("world: collected %zu/%zu chunks", len, len0);
}

void world::do_make_object(const bptr<object>& e, global_coords pos, bool sorted)
{
    fm_debug_assert(e->id != 0);
    fm_debug_assert(e->c);
    fm_debug_assert(pos.chunk3() == e->c->coord());
    fm_debug_assert(_unique_id && e->c->world()._unique_id == _unique_id);
    fm_assert(e->type() != object_type::none);
    const_cast<global_coords&>(e->coord) = pos;
    auto [_, fresh] = _objects->try_emplace(e->id, e);
    if (!fresh) [[unlikely]]
        fm_throw("object already initialized id:{}"_cf, e->id);
    if (sorted)
        e->c->add_object(e);
    else
        e->c->add_object_unsorted(e);
}

void world::erase_object(object_id id)
{
    fm_debug_assert(id != 0);
    auto cnt = _objects->erase(id);
    fm_debug_assert(cnt > 0);
}

bptr<object> world::find_object_(object_id id)
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
    fm_throw("object {} has wrong object type {}, should be {}"_cf, id, (size_t)actual, (size_t)expected);
}

void world::throw_on_wrong_scenery_type(object_id id, scenery_type actual, scenery_type expected)
{
    fm_throw("object {} has wrong scenery type {}, should be {}"_cf, id, (size_t)actual, (size_t)expected);
}

void world::throw_on_empty_scenery_proto(object_id id, global_coords pos, Vector2b offset)
{
    ERR_nospace << "scenery_proto subtype not set"
                << " id:" << id
                << " pos:" << point{pos, offset};
    auto ch = Vector3i(pos.chunk3());
    auto t = Vector2i(pos.local());
    fm_throw("scenery_proto subtype not set! id:{} chunk:{}x{}x{} tile:{}x{}"_cf, id, ch.x(), ch.y(), ch.z(), t.x(), t.y());
}

auto world::neighbors(chunk_coords_ coord) -> std::array<chunk*, 8>
{
    std::array<chunk*, 8> ret;
    for (auto i = 0u; i < 8; i++)
        ret[i] = at(coord + neighbor_offsets[i]);
    return ret;
}

critter_proto world::make_player_proto()
{
    critter_proto cproto;
    cproto.name = "Player"_s;
    cproto.speed = 10;
    cproto.playable = true;
    return cproto;
}

bool world::is_teardown() const { return _teardown; }

void world::init_scripts()
{
    fm_assert(!_script_initialized);
    _script_initialized = true;
    for (auto& [k, c] : _chunks)
        for (const auto& obj : c.objects())
            obj->init_script(obj);
}

void world::finish_scripts()
{
    fm_assert(_script_initialized);
    fm_assert(!_script_finalized);
    _script_finalized = true;

    for (auto& [k, c] : _chunks)
        for (const auto& obj : c.objects())
            obj->destroy_script_pre(obj, script_destroy_reason::quit);
    for (auto& [k, c] : _chunks)
        for (const auto& obj : c.objects())
            obj->destroy_script_post();
}

struct world::script_status world::script_status() const
{
    return { _script_initialized, _script_finalized };
}

bptr<critter> world::ensure_player_character(object_id& id)
{
    return ensure_player_character(id, make_player_proto());
}

bptr<critter> world::ensure_player_character(object_id& id_, critter_proto p)
{
    if (id_)
    {
        bptr<critter> tmp;
        if (auto C = find_object(id_); C && C->type() == object_type::critter)
        {
            auto ptr = static_pointer_cast<critter>(C);
            return {ptr};
        }
    }
    id_ = 0;

    auto id = (object_id)-1;

    bptr<critter> ret;

    for (auto& [coord, c] : chunks())
    {
        for (const auto& eʹ : c.objects())
        {
            const auto& e = *eʹ;
            if (e.type() == object_type::critter)
            {
                const auto& C = static_cast<const critter&>(e);
                if (C.playable)
                {
                    id = std::min(id, C.id);
                    ret = static_pointer_cast<critter>(eʹ);
                }
            }
        }
    }

    if (id != (object_id)-1)
        id_ = id;
    else
    {
        p.playable = true;
        ret = make_object<critter>(make_id(), global_coords{}, move(p));
        id_ = ret->id;
    }
    fm_debug_assert(ret);
    return ret;
}

template<typename T>
bptr<T> world::find_object(object_id id)
{
    static_assert(std::is_base_of_v<object, T>);
    if (bptr<object> ptr = find_object_(id); !ptr)
        return {};
    else if constexpr(std::is_same_v<T, object>)
        return ptr;
    else if (ptr->type() != object_type_<T>::value) [[unlikely]]
        throw_on_wrong_object_type(id, ptr->type(), object_type_<T>::value);
    else
        return static_pointer_cast<T>(move(ptr));
}

template<typename T>
requires is_strict_base_of<scenery, T>
bptr<T> world::find_object(object_id id)
{
    if (auto ptr = find_object<scenery>(id); !ptr)
        return {};
    else if (ptr->scenery_type() != scenery_type_<T>::value) [[unlikely]]
        throw_on_wrong_scenery_type(id, ptr->scenery_type(), scenery_type_<T>::value);
    else
        return static_pointer_cast<T>(move(ptr));
}

template bptr<object>  world::find_object<object>(object_id id);
template bptr<critter> world::find_object<critter>(object_id id);
template bptr<scenery> world::find_object<scenery>(object_id id);
template bptr<light>   world::find_object<light>(object_id id);
template bptr<generic_scenery> world::find_object<generic_scenery>(object_id id);
template bptr<door_scenery> world::find_object<door_scenery>(object_id id);

template<bool sorted>
bptr<scenery> world::make_scenery(object_id id, global_coords pos, scenery_proto&& proto)
{
    using type = bptr<scenery>;

    return std::visit(overloaded {
        [&](std::monostate) -> type {
            throw_on_empty_scenery_proto(id, pos, proto.offset);
        },
        [&](generic_scenery_proto&& p) -> type {
            return make_object<generic_scenery, sorted>(id, pos, move(p), move(proto));
        },
        [&](door_scenery_proto&& p) -> type {
            return make_object<door_scenery, sorted>(id, pos, move(p), move(proto));
        },
    }, move(proto.subtype));
}

template bptr<scenery> world::make_scenery<false>(object_id id, global_coords pos, scenery_proto&& proto);
template bptr<scenery> world::make_scenery<true>(object_id id, global_coords pos, scenery_proto&& proto);

} // namespace floormat
