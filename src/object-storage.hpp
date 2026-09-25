#pragma once
#include "object-storage-fwd.hpp"
#include "compat/defs.hpp"
#include "compat/borrowed-ptr.hpp"
#include "compat/is-one-of.hpp"
#include "object-type.hpp"
#include "scenery-type.hpp"
#include <concepts>
#include <type_traits>
#include <cr/Array.h>

namespace floormat {

// none and scenery aren't classes
constexpr inline uint32_t object_class_count = (uint32_t)object_type::COUNT - 2 + (uint32_t)scenery_type::COUNT - 1;

template<typename C>
constexpr inline bool is_object_class = is_one_of<C, generic_scenery, door_scenery, critter, light, hole>;

template<typename C, typename T, typename... Ts>
constexpr inline bool object_class_selected = (std::is_same_v<T, object> && sizeof...(Ts) == 0) || is_one_of<C, T, Ts...>;

template<typename F, typename C, typename Arg, typename T, typename... Ts>
concept visits_if_selected = !object_class_selected<C, T, Ts...> || std::invocable<const F&, Arg>;

template<typename F, template<typename> typename Arg, typename T, typename... Ts>
concept visits_selected_classes =
    visits_if_selected<F, generic_scenery, Arg<generic_scenery>, T, Ts...> &&
    visits_if_selected<F, door_scenery, Arg<door_scenery>, T, Ts...> &&
    visits_if_selected<F, critter, Arg<critter>, T, Ts...> &&
    visits_if_selected<F, light, Arg<light>, T, Ts...> &&
    visits_if_selected<F, hole, Arg<hole>, T, Ts...>;

namespace detail_object_storage {
template<typename C> using ref = C&;
template<typename C> using const_ref = const C&;
template<typename C> using list_ref = object_list<C>&;
} // namespace detail_object_storage

template<typename F, typename T, typename... Ts>
concept object_visitor = visits_selected_classes<F, detail_object_storage::ref, T, Ts...>;

template<typename F, typename T, typename... Ts>
concept const_object_visitor = visits_selected_classes<F, detail_object_storage::const_ref, T, Ts...>;

template<typename F, typename T, typename... Ts>
concept object_list_visitor = visits_selected_classes<F, detail_object_storage::list_ref, T, Ts...>;

template<typename T>
class object_range final
{
    const bptr<object>* _begin;
    const bptr<object>* _end;

public:
    class iterator final
    {
        const bptr<object>* _p;

    public:
        explicit iterator(const bptr<object>* p) noexcept;
        T& operator*() const noexcept;
        bool operator==(const iterator&) const noexcept;
        iterator& operator++() noexcept;
    };

    object_range(const bptr<object>* begin, const bptr<object>* end) noexcept;
    iterator begin() const noexcept;
    iterator end() const noexcept;
    uint32_t size() const noexcept;
};

class object_list_base
{
    friend class object_storage;

protected:
    Array<bptr<object>> _items;

public:
    inline uint32_t size() const noexcept;
    inline const bptr<object>& ptr(uint32_t i) noexcept; // non-const only: a bptr yields a mutable object
};

template<typename T>
class object_list final : public object_list_base
{
public:
    T& operator[](uint32_t i) noexcept;
    const T& operator[](uint32_t i) const noexcept;

    object_range<T> range() noexcept;
    object_range<const T> range() const noexcept;
    auto begin() noexcept;
    auto end() noexcept;
    auto begin() const noexcept;
    auto end() const noexcept;
};

class object_storage final
{
    friend class chunk;
    template<bool Const> class iterator_;

    // for_each_list() visits in this order: doors update before critters
    object_list<generic_scenery> _generic;
    object_list<door_scenery> _door;
    object_list<critter> _critter;
    object_list<light> _light;
    object_list<hole> _hole;
    bool _sorted = true;

    template<typename T, typename... Ts, typename Self, typename F> void for_each_list(this Self& self, F&& fn);
    object_list_base& list_for(object_type type, scenery_type sc_type) noexcept;
    const object_list_base& list_for(object_type type, scenery_type sc_type) const noexcept;
    const object_list_base* list_at(uint32_t k) const noexcept;

    [[nodiscard]] size_t insert(const bptr<object>& e);
    void append(const bptr<object>& e);
    void erase(const object& e, size_t i);
    [[nodiscard]] bool sort();
    void clear();

public:
    using iterator = iterator_<false>;
    using const_iterator = iterator_<true>;

    object_storage() noexcept;
    ~object_storage() noexcept;
    fm_DISABLE_MOVE_COPY(object_storage);

    template<typename C> object_list<C>& list() noexcept;
    template<typename C> const object_list<C>& list() const noexcept;

    template<typename T = object, typename... Ts, object_visitor<T, Ts...> F> void visit(const F& fn);
    template<typename T = object, typename... Ts, const_object_visitor<T, Ts...> F> void visit(const F& fn) const;
    template<typename T = object, typename... Ts, object_list_visitor<T, Ts...> F> void visit_lists(const F& fn);

    uint32_t size() const noexcept;
    bool empty() const noexcept;
    size_t index_of(const object& e) const;
    const bptr<object>& ptr(const object& e, size_t i) const;

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const bptr<object>& operator[](uint32_t i) noexcept;
    const object& operator[](uint32_t i) const noexcept;
};

template<bool Const>
class object_storage::iterator_ final
{
    friend class object_storage;

    const object_storage* _s = nullptr;
    const bptr<object>* _p = nullptr;
    const bptr<object>* _end = nullptr;
    uint32_t _k = 0;

    explicit iterator_(const object_storage& s) noexcept;
    void load(uint32_t k) noexcept;

public:
    iterator_() noexcept;
    bool operator==(const iterator_& other) const noexcept;
    decltype(auto) operator*() const noexcept;
    iterator_& operator++() noexcept;
};

} // namespace floormat
