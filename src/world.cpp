#include "world.hpp"
#include "chunk.hpp"
#include "chunk-table.hpp"
#include "object.hpp"
#include "critter.hpp"
#include "scenery.hpp"
#include "scenery-proto.hpp"
#include "light.hpp"
#include "hole.hpp"
#include "grid-pass.hpp"
#include "grid-pass-pool.hpp"
#include "search-constants.hpp"
#include "tile-defs.hpp"
#include "compat/array-size.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/hash.hpp"
#include "compat/exception.hpp"
#include "compat/overloaded.hpp"
#include "compat/hash-table-load-factor.hpp"
#include "compat/non-const.hpp"
#include <cr/Pointer.h>
#include <cr/GrowableArray.h>
#include <gtl/phmap.hpp>

using namespace floormat;

size_t world::object_id_hasher::operator()(object_id id) const noexcept { return gtl::Hash<object_id>{}(id); }

namespace floormat {

struct world::Impl
{
    gtl::flat_hash_map<object_id, bptr<object>, object_id_hasher> _objects;
    Pointer<Pass::PoolRegistry> _pass_registry;
    Pointer<Pass::Pool> _cover_pass_pool;
};

Grid::Pass::PoolRegistry& world::pass_pool_registry()
{
    if (!impl->_pass_registry)
        impl->_pass_registry.reset(new Grid::Pass::PoolRegistry{(uint32_t)Search::div_size.x()});
    return *impl->_pass_registry;
}

Grid::Pass::Pool& world::cover_pass_pool()
{
    if (!impl->_cover_pass_pool)
        impl->_cover_pass_pool.reset(new Grid::Pass::Pool{Grid::Pass::Params{8, 8}.validate()});
    return *impl->_cover_pass_pool;
}

Grid::Pass::Pool& world::raycast_pass_pool()
{
    return pass_pool_registry().pool_for(tile_size_xy);
}

world::world() : _unique_id{InPlace}
{
    auto& impl = *this->impl;
    Hash::set_open_addressing_load_factor(impl._objects);
}

world::world(world&& w) noexcept :
    _last_chunk{w._last_chunk},
    impl{move(w.impl)},
    _chunk_table{move(w._chunk_table)},
    _head{w._head},
    _tail{w._tail},
    _unique_id{move(w._unique_id)},
    _object_counter{w._object_counter},
    _current_frame{w._current_frame},
    _teardown{w._teardown},
    _script_initialized{w._script_initialized},
    _script_finalized{w._script_finalized}
{
    w._head = nullptr;
    w._tail = nullptr;
    w._last_chunk = {};
    w._object_counter = 0;
    for (chunk* c = _head; c; c = c->_next)
        c->_world = this;
}

world& world::operator=(world&& w) noexcept
{
    auto& impl = *this->impl;
    fm_debug_assert(&w != this);
    fm_assert(!w._script_initialized);
    fm_assert(!w._script_finalized);
    fm_assert(!_script_initialized || _script_finalized);
    _script_initialized = false;
    _script_finalized = false;
    fm_assert(!w._teardown);
    fm_assert(!_teardown);
    _last_chunk = {};
    impl._objects = move(w.impl->_objects);
    w.impl->_objects = {};

    // suppress unregister; _chunk_table is replaced wholesale below
    _teardown = true;
    while (_head)
    {
        chunk* next = _head->_next;
        delete _head;
        _head = next;
    }
    _tail = nullptr;
    _teardown = false;

    _chunk_table = move(w._chunk_table);
    _head = w._head;
    _tail = w._tail;
    w._head = nullptr;
    w._tail = nullptr;
    for (chunk* c = _head; c; c = c->_next)
        c->_world = this;

    fm_assert(w._unique_id);
    _unique_id = move(w._unique_id);
    fm_debug_assert(_unique_id);
    fm_debug_assert(w._unique_id == nullptr);
    _object_counter = w._object_counter;
    w._object_counter = 0;
    _current_frame = w._current_frame;
    return *this;
}

world::~world() noexcept
{
    fm_assert(_script_finalized || !_script_initialized);
    for (chunk* c = _head; c; c = c->_next)
        c->on_teardown();
    _teardown = true;
    impl->_objects.clear();
    chunk* c = _head;
    while (c)
    {
        chunk* next = c->_next;
        delete c;
        c = next;
    }
    _head = nullptr;
    _tail = nullptr;
    _last_chunk = {};
}

bool world::unique_id::operator==(const unique_id& other) const { return this == &other; }

chunk& world::operator[](chunk_coords_ coord) noexcept
{
    fm_debug_assert(coord.z >= chunk_z_min && coord.z <= chunk_z_max);
    auto& [c, coord2] = _last_chunk;
    if (coord != coord2)
    {
        c = _chunk_table->chunk_at(coord);
        if (!c)
            c = new chunk(*this, coord);
        coord2 = coord;
    }
    return *c;
}

chunk* world::at(chunk_coords_ c) noexcept
{
    return _chunk_table->chunk_at(c);
}

const chunk* world::at(chunk_coords_ c) const noexcept
{
    return _chunk_table->chunk_at(c);
}

bool world::contains(chunk_coords_ c) const noexcept
{
    return _chunk_table->chunk_at(c) != nullptr;
}

void world::clear()
{
    auto& impl = *this->impl;
    fm_assert(!_teardown);
    while (_head)
    {
        chunk* next = _head->_next;
        delete _head;
        _head = next;
    }
    _tail = nullptr;
    impl._objects.clear();
    Hash::set_open_addressing_load_factor(impl._objects);
    _object_counter = object_counter_init;
    _last_chunk = {};
}

void world::collect(bool force, bool quiet)
{
    size_t len0 = 0, deleted = 0;
    chunk* c = _head;
    while (c)
    {
        len0++;
        chunk* next = c->_next;
        if (c->empty(force))
        {
            delete c;
            deleted++;
        }
        c = next;
    }
    _last_chunk = {};
    chunk_table_prepare_frame();
    if (!quiet && deleted > 1)
        fm_debug("world: collected %zu/%zu chunks", deleted, len0);
}

size_t world::size() const noexcept
{
    size_t n = 0;
    for (const chunk* c = _head; c; c = c->_next)
        n++;
    return n;
}

template<typename Chunk>
world::chunks_iterator<Chunk>& world::chunks_iterator<Chunk>::operator++() noexcept
{
    _cur = _cur->_next;
    return *this;
}

template<typename Chunk>
world::chunks_iterator<Chunk> world::chunks_range<Chunk>::begin() const noexcept
{
    chunks_iterator<Chunk> it;
    it._cur = _head_;
    return it;
}

template class world::chunks_iterator<chunk>;
template class world::chunks_iterator<const chunk>;
template class world::chunks_range<chunk>;
template class world::chunks_range<const chunk>;

world::chunks_range<chunk> world::chunks() noexcept { return {_head}; }
world::chunks_range<const chunk> world::chunks() const noexcept { return {_head}; }

void world::do_make_object(const bptr<object>& e, global_coords pos, bool sorted)
{
    auto& impl = *this->impl;
    fm_debug_assert(e);
    fm_debug_assert(e->id != 0);
    fm_debug_assert(e->c);
    fm_debug_assert(pos.chunk3() == e->c->coord());
    fm_debug_assert(_unique_id && e->c->world()._unique_id == _unique_id);
    fm_assert(e->type() != object_type::none);
    const_cast<global_coords&>(e->coord) = pos;
    auto [_, fresh] = impl._objects.try_emplace(e->id, e);
    if (!fresh) [[unlikely]]
        fm_throw("object already initialized id:{}"_cf, e->id);
    if (sorted)
        e->c->add_object(e);
    else
        e->c->add_object_unsorted(e);
    Hash::set_open_addressing_load_factor(impl._objects);
}

void world::erase_object(object_id id)
{
    auto& impl = *this->impl;
    fm_debug_assert(id != 0);
    auto cnt = impl._objects.erase(id);
    fm_debug_assert(cnt > 0);
}

bptr<object> world::find_object_(object_id id)
{
    auto& impl = *this->impl;
    auto it = impl._objects.find(id);
    auto ret = it == impl._objects.end() ? nullptr : it->second;
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
    fm_throw("scenery_proto subtype not set! id:{} chunk:{}x{}x{} tile:{}x{}"_cf,
             id, ch.x(), ch.y(), ch.z(), t.x(), t.y());
}

std::array<chunk*, 8> world::neighbors(chunk_coords_ coord)
{
    return _chunk_table->neighbors(coord);
}

std::array<const chunk*, 8> world::neighbors(chunk_coords_ coord) const
{
    return _chunk_table->neighbors(coord);
}

void world::chunk_table_prepare_frame()
{
#ifndef FM_NO_DEBUG2
    _chunk_table->check_in_sync(*this);
#endif
}

void world::register_chunk(chunk* c) noexcept
{
    fm_debug_assert(c->_prev == nullptr && c->_next == nullptr);
    _chunk_table->update_slot(c->_coord, c);
    c->_prev = _tail;
    if (_tail)
        _tail->_next = c;
    else
        _head = c;
    _tail = c;
}

void world::unregister_chunk(chunk* c) noexcept
{
    if (_teardown)
        return;
    _chunk_table->update_slot(c->_coord, nullptr);
    if (c->_prev)
        c->_prev->_next = c->_next;
    else
        _head = c->_next;
    if (c->_next)
        c->_next->_prev = c->_prev;
    else
        _tail = c->_prev;
    c->_prev = nullptr;
    c->_next = nullptr;
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
    for (auto& c : chunks())
        for (const auto& obj : c.objects())
            obj->init_script(obj);
}

void world::finish_scripts()
{
    fm_assert(_script_initialized);
    fm_assert(!_script_finalized);
    _script_finalized = true;

    for (auto& c : chunks())
        for (const auto& obj : c.objects())
            obj->destroy_script_pre(obj, script_destroy_reason::quit);
    for (auto& c : chunks())
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

    for (auto& c : chunks())
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
bptr<const T> world::find_object(object_id id) const
{
    return non_const(*this).find_object<T>(id);
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

template<typename T>
requires is_strict_base_of<scenery, T>
bptr<const T> world::find_object(object_id id) const
{
    return non_const(*this).find_object<T>(id);
}

template bptr<object>  world::find_object<object>(object_id id);
template bptr<critter> world::find_object<critter>(object_id id);
template bptr<scenery> world::find_object<scenery>(object_id id);
template bptr<light>   world::find_object<light>(object_id id);
template bptr<hole>   world::find_object<hole>(object_id id);
template bptr<generic_scenery> world::find_object<generic_scenery>(object_id id);
template bptr<door_scenery> world::find_object<door_scenery>(object_id id);

template bptr<const object>  world::find_object<object>(object_id id) const;
template bptr<const critter> world::find_object<critter>(object_id id) const;
template bptr<const scenery> world::find_object<scenery>(object_id id) const;
template bptr<const light>   world::find_object<light>(object_id id) const;
template bptr<const hole>   world::find_object<hole>(object_id id) const;
template bptr<const generic_scenery> world::find_object<generic_scenery>(object_id id) const;
template bptr<const door_scenery> world::find_object<door_scenery>(object_id id) const;

template<bool sorted>
bptr<scenery> world::make_scenery(object_id id, global_coords pos, scenery_proto&& proto)
{
    using type = bptr<scenery>;

    return swl::visit(overloaded {
        [&](swl::monostate) -> type {
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
