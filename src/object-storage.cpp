#include "object-storage.inl"
#include "object.hpp"
#include "scenery.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/fwd/algorithm/sort.hpp"
#include "compat/fwd/algorithm/lower_bound.hpp"
#include <array>
#include <utility>
#include <cr/GrowableArray.h>

namespace floormat {

namespace {

constexpr auto object_id_lessp = [](const bptr<object>& a, const bptr<object>& b) { return a->id < b->id; };

enum scenery_type scenery_type_of(const object& e)
{
    if (e.type() != object_type::scenery)
        return scenery_type::none;
    return static_cast<const scenery&>(e).scenery_type();
}

template<auto List>
const object_list_base& list_member(const object_storage& s) noexcept
{
    return s.*List;
}

} // namespace

object_storage::object_storage() noexcept = default;
object_storage::~object_storage() noexcept = default;

const object_list_base& object_storage::list_for(object_type type, scenery_type sc_type) const noexcept
{
    using getter = const object_list_base& (*)(const object_storage&) noexcept;
    static constexpr auto object_types = []
    {
        std::array<std::array<getter, (usize)scenery_type::COUNT>, (usize)object_type::COUNT> array{};
        array[(usize)object_type::scenery][(usize)scenery_type::generic] = &list_member<&object_storage::_generic>;
        array[(usize)object_type::scenery][(usize)scenery_type::door] = &list_member<&object_storage::_door>;
        array[(usize)object_type::critter][(usize)scenery_type::none] = &list_member<&object_storage::_critter>;
        array[(usize)object_type::light][(usize)scenery_type::none] = &list_member<&object_storage::_light>;
        array[(usize)object_type::hole][(usize)scenery_type::none] = &list_member<&object_storage::_hole>;
        return array;
    }();

    getter fn = nullptr;
    if ((usize)type < object_types.size() && (usize)sc_type < object_types[0].size())
        fn = object_types[(usize)type][(usize)sc_type];
    if (!fn) [[unlikely]]
        fm_abort("invalid object type %d/%d", (int)type, (int)sc_type);
    return fn(*this);
}

object_list_base& object_storage::list_for(object_type type, scenery_type sc_type) noexcept
{
    return non_const(std::as_const(*this).list_for(type, sc_type));
}

const object_list_base* object_storage::list_at(u32 k) const noexcept
{
    const object_list_base* ret = nullptr;
    u32 n = 0;
    for_each_list<object>([&](const object_list_base& l) {
        if (n++ == k)
            ret = &l;
    });
    return ret;
}

usize object_storage::insert(const bptr<object>& e)
{
    fm_assert(_sorted);
    auto& es = list_for(e->type(), scenery_type_of(*e))._items;
    arrayReserve(es, 8);
    auto* it = std::lower_bound(es.data(), es.data() + es.size(), e, object_id_lessp);
    auto i = (usize)(it - es.data());
    arrayInsert(es, i, e);
    return i;
}

void object_storage::append(const bptr<object>& e)
{
    _sorted = false;
    auto& es = list_for(e->type(), scenery_type_of(*e))._items;
    arrayReserve(es, 8);
    arrayAppend(es, e);
}

void object_storage::erase(const object& e, usize i)
{
    fm_assert(_sorted);
    auto& es = list_for(e.type(), scenery_type_of(e))._items;
    fm_assert(i < es.size());
    fm_assert(&*es[i] == &e);
    arrayRemove(es, i);
}

bool object_storage::sort()
{
    if (_sorted)
        return false;
    _sorted = true;
    for_each_list<object>([](object_list_base& l) {
        std::sort(l._items.begin(), l._items.end(), object_id_lessp);
    });
    return true;
}

void object_storage::clear()
{
    for_each_list<object>([](object_list_base& l) {
        arrayResize(l._items, 0);
        arrayShrink(l._items);
    });
}

u32 object_storage::size() const noexcept
{
    u32 n = 0;
    for_each_list<object>([&](const object_list_base& l) { n += l.size(); });
    return n;
}

bool object_storage::empty() const noexcept { return size() == 0; }

usize object_storage::index_of(const object& e) const
{
    fm_assert(_sorted);
    const auto fn = [id = e.id](const bptr<object>& a, std::nullptr_t) { return a->id < id; };
    const auto& es = list_for(e.type(), scenery_type_of(e))._items;
    auto it = std::lower_bound(es.cbegin(), es.cend(), nullptr, fn);
    fm_assert(it != es.cend());
    fm_assert((*it)->id == e.id);
    return (usize)(it - es.cbegin());
}

const bptr<object>& object_storage::ptr(const object& e, usize i)
{
    const auto& es = list_for(e.type(), scenery_type_of(e))._items;
    fm_assert(i < es.size());
    const auto& p = es[i];
    fm_assert(&*p == &e);
    return p;
}

template<bool Const>
object_storage::iterator_<Const>::iterator_(const object_storage& s) noexcept : _s{&s}
{
    load(0);
}

template<bool Const>
void object_storage::iterator_<Const>::load(u32 k) noexcept
{
    for (; const auto* l = _s->list_at(k); k++)
    {
        if (l->size() != 0)
        {
            _k = k;
            _p = l->_items.data();
            _end = _p + l->size();
            return;
        }
    }
    _p = _end = nullptr;
}

template class object_storage::iterator_<false>;
template class object_storage::iterator_<true>;

object_storage::iterator object_storage::begin() noexcept
{
    fm_assert(_sorted);
    return iterator{*this};
}

object_storage::iterator object_storage::end() noexcept { return {}; }

object_storage::const_iterator object_storage::begin() const noexcept
{
    fm_assert(_sorted);
    return const_iterator{*this};
}

object_storage::const_iterator object_storage::end() const noexcept { return {}; }

const bptr<object>& object_storage::item_at(u32 i) const noexcept
{
    const bptr<object>* ret = nullptr;
    for_each_list<object>([&](const object_list_base& l) {
        if (ret)
            return;
        if (i < l.size())
            ret = &l._items[i];
        else
            i -= l.size();
    });
    fm_assert(ret);
    return *ret;
}

const bptr<object>& object_storage::operator[](u32 i) noexcept { return item_at(i); }
const object& object_storage::operator[](u32 i) const noexcept { return *item_at(i); }

} // namespace floormat
