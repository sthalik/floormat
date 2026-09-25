#pragma once
#include "object-storage.hpp"
#include "compat/assert.hpp"
#include "compat/non-const.hpp"

namespace floormat {

template<typename T>
object_range<T>::iterator::iterator(const bptr<object>* p) noexcept : _p{p} {}

template<typename T>
T& object_range<T>::iterator::operator*() const noexcept
{
    return static_cast<T&>(**_p);
}

template<typename T>
bool object_range<T>::iterator::operator==(const iterator&) const noexcept = default;

template<typename T>
auto object_range<T>::iterator::operator++() noexcept -> iterator&
{
    ++_p;
    return *this;
}

template<typename T>
object_range<T>::object_range(const bptr<object>* begin, const bptr<object>* end) noexcept : _begin{begin}, _end{end} {}

template<typename T> auto object_range<T>::begin() const noexcept -> iterator { return iterator{_begin}; }
template<typename T> auto object_range<T>::end() const noexcept -> iterator { return iterator{_end}; }
template<typename T> uint32_t object_range<T>::size() const noexcept { return (uint32_t)(_end - _begin); }

inline uint32_t object_list_base::size() const noexcept { return (uint32_t)_items.size(); }
inline const bptr<object>& object_list_base::ptr(uint32_t i) noexcept { return _items[i]; }

template<typename T> T& object_list<T>::operator[](uint32_t i) noexcept { return static_cast<T&>(*_items[i]); }
template<typename T> const T& object_list<T>::operator[](uint32_t i) const noexcept { return static_cast<const T&>(*_items[i]); }

template<typename T>
object_range<T> object_list<T>::range() noexcept
{
    return {_items.data(), _items.data() + _items.size()};
}

template<typename T>
object_range<const T> object_list<T>::range() const noexcept
{
    return {_items.data(), _items.data() + _items.size()};
}

template<typename T> auto object_list<T>::begin() noexcept { return range().begin(); }
template<typename T> auto object_list<T>::end() noexcept { return range().end(); }
template<typename T> auto object_list<T>::begin() const noexcept { return range().begin(); }
template<typename T> auto object_list<T>::end() const noexcept { return range().end(); }

template<bool Const>
object_storage::iterator_<Const>::iterator_() noexcept = default;

template<bool Const>
bool object_storage::iterator_<Const>::operator==(const iterator_& other) const noexcept
{
    return _p == other._p;
}

template<bool Const>
decltype(auto) object_storage::iterator_<Const>::operator*() const noexcept
{
    if constexpr (Const)
        return static_cast<const object&>(**_p);
    else
        return *_p;
}

template<bool Const>
auto object_storage::iterator_<Const>::operator++() noexcept -> iterator_&
{
    if (++_p == _end) [[unlikely]]
        load(_k + 1);
    return *this;
}

template<typename T, typename... Ts, typename Self, typename F>
void object_storage::for_each_list(this Self& self, F&& fn)
{
    static_assert(!is_one_of<object, T, Ts...> || sizeof...(Ts) == 0, "object selects every class; name no other");
    static_assert(std::is_same_v<T, object> || (is_object_class<T> && ... && is_object_class<Ts>), "not a final object class");
    static_assert(types_are_distinct<T, Ts...>, "class named twice");
    static_assert(object_class_count == 5, "add the new class to each list of classes in object-storage.hpp and .inl");
    if constexpr (object_class_selected<generic_scenery, T, Ts...>)
        fn(self._generic);
    if constexpr (object_class_selected<door_scenery, T, Ts...>)
        fn(self._door);
    if constexpr (object_class_selected<critter, T, Ts...>)
        fn(self._critter);
    if constexpr (object_class_selected<light, T, Ts...>)
        fn(self._light);
    if constexpr (object_class_selected<hole, T, Ts...>)
        fn(self._hole);
}

template<typename C>
object_list<C>& object_storage::list() noexcept
{
    if constexpr (std::is_same_v<C, generic_scenery>)
        return _generic;
    else if constexpr (std::is_same_v<C, door_scenery>)
        return _door;
    else if constexpr (std::is_same_v<C, critter>)
        return _critter;
    else if constexpr (std::is_same_v<C, light>)
        return _light;
    else
    {
        static_assert(std::is_same_v<C, hole>, "not a final object class");
        return _hole;
    }
}

template<typename C>
const object_list<C>& object_storage::list() const noexcept
{
    return non_const(*this).list<C>();
}

template<typename T, typename... Ts, object_visitor<T, Ts...> F>
void object_storage::visit(const F& fn)
{
    fm_assert(_sorted);
    for_each_list<T, Ts...>([&]<typename C>(object_list<C>& l) {
        static_assert(std::invocable<const F&, C&>);
        static_assert(std::is_void_v<std::invoke_result_t<const F&, C&>>, "visit() can't stop early");
        const auto* const data = l._items.data();
        const auto size = l.size();
        for (auto i = 0u; i < size; i++)
        {
            fn(l[i]);
            fm_debug_assert(l._items.data() == data && l.size() == size);
        }
    });
}

template<typename T, typename... Ts, const_object_visitor<T, Ts...> F>
void object_storage::visit(const F& fn) const
{
    fm_assert(_sorted);
    for_each_list<T, Ts...>([&]<typename C>(const object_list<C>& l) {
        static_assert(std::invocable<const F&, const C&>);
        static_assert(std::is_void_v<std::invoke_result_t<const F&, const C&>>, "visit() can't stop early");
        const auto* const data = l._items.data();
        const auto size = l.size();
        for (auto i = 0u; i < size; i++)
        {
            fn(l[i]);
            fm_debug_assert(l._items.data() == data && l.size() == size);
        }
    });
}

template<typename T, typename... Ts, object_list_visitor<T, Ts...> F>
void object_storage::visit_lists(const F& fn)
{
    for_each_list<T, Ts...>(fn);
}

} // namespace floormat
