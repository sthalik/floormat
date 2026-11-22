#pragma once
#include "object-id.hpp"
#include "compat/defs.hpp"
#include "compat/is-one-of.hpp"
#include "entity/component.hpp"
#include <tuple>
#include <utility>
#include <cr/Array.h>

namespace floormat::detail::component_table {

template<typename Component>
concept has_component_fields = requires { component_fields<Component>::fields(); };

template<typename Fields> struct field_arrays;

template<> struct field_arrays<no_fields>
{
    using type = std::tuple<>;
};

template<auto COUNT, typename... Fields> struct field_arrays<field_list<COUNT, Fields...>>
{
    using type = std::tuple<Array<typename Fields::type>...>;
};

template<typename Component>
using field_arrays_of = field_arrays<detail::component::field_list_of<Component>>::type;

} // namespace floormat::detail::component_table

namespace floormat {

// Refers to one row of a component_table. Inserting, erasing or moving any row of its table invalidates it.
template<typename Table>
class component_row_ref_ final
{
    Table* _table;
    u32 _row;

public:
    component_row_ref_(Table& table, u32 row) noexcept;

    object_id id() const noexcept;
    template<auto Enumerator> auto& get() const noexcept;
};

// Holds one row per object id, sorted by id.
template<typename... Components>
class component_table final
{
    static_assert(types_are_distinct<Components...>, "a component is listed twice");
    static_assert((detail::component_table::has_component_fields<Components> && ...),
                  "a component has no component_fields specialization");

    using arrays_tuple = decltype(std::tuple_cat(
        std::declval<std::tuple<Array<object_id>>>(),
        std::declval<detail::component_table::field_arrays_of<Components>>()...));

    // Element 0 holds the ids and element 1 + field_index holds that field. All have one size and one capacity.
    arrays_tuple _arrays;

    u32 lower_bound(object_id id) const noexcept;
    void make_room_for_row();

public:
    using row_ref = component_row_ref_<component_table>;
    using const_row_ref = component_row_ref_<const component_table>;

    component_table() noexcept = default;
    ~component_table() noexcept = default;
    fm_DISABLE_COPY(component_table);
    fm_DEFAULT_MOVE_(component_table);

    u32 size() const noexcept;
    bool empty() const noexcept;
    ArrayView<const object_id> ids() const noexcept;

    // Returns the new row's index. Its fields are value-initialized.
    u32 insert_row(object_id id);
    Optional<u32> find_row(object_id id) const noexcept;
    void erase_row(u32 row);
    // Returns the row's index in dst.
    u32 move_row_to(component_table& dst, u32 row);

    // Inserting, erasing or moving a row, reserve() and clear() invalidate the view.
    template<auto Enumerator> ArrayView<field_type<Enumerator, Components...>> field_array() noexcept;
    template<auto Enumerator> ArrayView<const field_type<Enumerator, Components...>> field_array() const noexcept;
    template<auto Enumerator> field_type<Enumerator, Components...>& get(u32 row) noexcept;
    template<auto Enumerator> const field_type<Enumerator, Components...>& get(u32 row) const noexcept;

    row_ref operator[](u32 row) noexcept;
    const_row_ref operator[](u32 row) const noexcept;

    void reserve(u32 capacity);
    void clear();
};

template<typename Table>
component_row_ref_<Table>::component_row_ref_(Table& table, u32 row) noexcept: _table{&table}, _row{row} {}

template<typename Table>
object_id component_row_ref_<Table>::id() const noexcept
{
    return _table->ids()[_row];
}

template<typename Table>
template<auto Enumerator>
auto& component_row_ref_<Table>::get() const noexcept
{
    return _table->template get<Enumerator>(_row);
}

template<typename... Components>
u32 component_table<Components...>::size() const noexcept
{
    return (u32)std::get<0>(_arrays).size();
}

template<typename... Components>
bool component_table<Components...>::empty() const noexcept
{
    return size() == 0;
}

template<typename... Components>
ArrayView<const object_id> component_table<Components...>::ids() const noexcept
{
    return std::get<0>(_arrays);
}

template<typename... Components>
template<auto Enumerator>
ArrayView<field_type<Enumerator, Components...>> component_table<Components...>::field_array() noexcept
{
    return std::get<1 + field_index<Enumerator, Components...>>(_arrays);
}

template<typename... Components>
template<auto Enumerator>
ArrayView<const field_type<Enumerator, Components...>> component_table<Components...>::field_array() const noexcept
{
    return std::get<1 + field_index<Enumerator, Components...>>(_arrays);
}

template<typename... Components>
template<auto Enumerator>
field_type<Enumerator, Components...>& component_table<Components...>::get(u32 row) noexcept
{
    return field_array<Enumerator>()[row];
}

template<typename... Components>
template<auto Enumerator>
const field_type<Enumerator, Components...>& component_table<Components...>::get(u32 row) const noexcept
{
    return field_array<Enumerator>()[row];
}

template<typename... Components>
auto component_table<Components...>::operator[](u32 row) noexcept -> row_ref
{
    return row_ref{*this, row};
}

template<typename... Components>
auto component_table<Components...>::operator[](u32 row) const noexcept -> const_row_ref
{
    return const_row_ref{*this, row};
}

} // namespace floormat
