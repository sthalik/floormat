#include "object-storage.inl"
#include "object.hpp"
#include "scenery.hpp"
#include "compat/borrowed-ptr.inl"
#include <array>
#include <algorithm>
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
        std::array<std::array<getter, (size_t)scenery_type::COUNT>, (size_t)object_type::COUNT> array{};
        array[(size_t)object_type::scenery][(size_t)scenery_type::generic] = &list_member<&object_storage::_generic>;
        array[(size_t)object_type::scenery][(size_t)scenery_type::door] = &list_member<&object_storage::_door>;
        array[(size_t)object_type::critter][(size_t)scenery_type::none] = &list_member<&object_storage::_critter>;
        array[(size_t)object_type::light][(size_t)scenery_type::none] = &list_member<&object_storage::_light>;
        array[(size_t)object_type::hole][(size_t)scenery_type::none] = &list_member<&object_storage::_hole>;
        return array;
    }();

    getter fn = nullptr;
    if ((size_t)type < object_types.size() && (size_t)sc_type < object_types[0].size())
        fn = object_types[(size_t)type][(size_t)sc_type];
    if (!fn) [[unlikely]]
        fm_abort("invalid object type %d/%d", (int)type, (int)sc_type);
    return fn(*this);
}

object_list_base& object_storage::list_for(object_type type, scenery_type sc_type) noexcept
{
    return non_const(std::as_const(*this).list_for(type, sc_type));
}

const object_list_base* object_storage::list_at(uint32_t k) const noexcept
{
    const object_list_base* ret = nullptr;
    uint32_t n = 0;
    for_each_list<object>([&](const object_list_base& l) {
        if (n++ == k)
            ret = &l;
    });
    return ret;
}

size_t object_storage::insert(const bptr<object>& e)
{
    fm_assert(_sorted);
    auto& es = list_for(e->type(), scenery_type_of(*e))._items;
    arrayReserve(es, 8);
    auto* it = std::lower_bound(es.data(), es.data() + es.size(), e, object_id_lessp);
    auto i = (size_t)(it - es.data());
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

void object_storage::erase(const object& e, size_t i)
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

uint32_t object_storage::size() const noexcept
{
    uint32_t n = 0;
    for_each_list<object>([&](const object_list_base& l) { n += l.size(); });
    return n;
}

bool object_storage::empty() const noexcept { return size() == 0; }

size_t object_storage::index_of(const object& e) const
{
    fm_assert(_sorted);
    const auto fn = [id = e.id](const bptr<object>& a, std::nullptr_t) { return a->id < id; };
    const auto& es = list_for(e.type(), scenery_type_of(e))._items;
    auto it = std::lower_bound(es.cbegin(), es.cend(), nullptr, fn);
    fm_assert(it != es.cend());
    fm_assert((*it)->id == e.id);
    return (size_t)(it - es.cbegin());
}

const bptr<object>& object_storage::ptr(const object& e, size_t i) const
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
void object_storage::iterator_<Const>::load(uint32_t k) noexcept
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

const bptr<object>& object_storage::operator[](uint32_t i) noexcept
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

const object& object_storage::operator[](uint32_t i) const noexcept { return *non_const(*this)[i]; }

} // namespace floormat
