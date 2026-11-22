#pragma once
#include <type_traits>

namespace floormat {

// Names one field of a component by its enumerator and gives the type stored for it.
template<auto Enumerator, typename T> struct component_field
{
    using type = T;
    static constexpr auto enumerator = Enumerator;
};

// Lists a component's fields in enumerator order. COUNT is the field enum's COUNT enumerator and must be passed.
template<auto COUNT, typename... Fields> struct field_list;

// Returned by fields() of a component that stores nothing and only marks objects.
struct no_fields {};

// Each component specializes this. Its fields() returns a field_list<...>{} or no_fields{}.
template<typename Component> struct component_fields;

} // namespace floormat

namespace floormat::detail::component {

template<u32 I, typename T, typename... Ts> struct nth_type : nth_type<I - 1, Ts...> {};
template<typename T, typename... Ts> struct nth_type<0, T, Ts...> { using type = T; };

template<typename E, typename Field>
consteval bool is_enumerator_with_value(usize value)
{
    if constexpr (std::is_same_v<std::remove_const_t<decltype(Field::enumerator)>, E>)
        return (usize)Field::enumerator == value;
    else
        return false;
}

template<auto COUNT, typename... Fields>
consteval bool is_field_list_in_enumerator_order()
{
    using E = decltype(COUNT);
    if constexpr (!std::is_enum_v<E>)
        return false;
    else if constexpr (sizeof...(Fields) != (usize)COUNT)
        return false;
    else
    {
        usize i = 0;
        return (is_enumerator_with_value<E, Fields>(i++) && ...);
    }
}

} // namespace floormat::detail::component

namespace floormat {

template<auto COUNT, typename... Fields>
constexpr inline bool is_field_list_in_enumerator_order =
    detail::component::is_field_list_in_enumerator_order<COUNT, Fields...>();

template<auto COUNT, typename... Fields>
struct field_list
{
    static_assert(std::is_enum_v<decltype(COUNT)>, "COUNT must be the field enum's COUNT enumerator");
    static_assert(sizeof...(Fields) == (usize)COUNT, "a field_list needs one field per enumerator below COUNT");
    static_assert(!std::is_enum_v<decltype(COUNT)> || sizeof...(Fields) != (usize)COUNT ||
                  is_field_list_in_enumerator_order<COUNT, Fields...>,
                  "field i must name the field enum's enumerator of value i");
    static_assert((std::is_object_v<typename Fields::type> && ...), "a field's type must be an object type");
    static_assert((std::is_nothrow_move_constructible_v<typename Fields::type> && ...),
                  "a field's type must be nothrow move constructible");
    static_assert(((std::is_trivially_copyable_v<typename Fields::type> ||
                    std::is_nothrow_move_assignable_v<typename Fields::type>) && ...),
                  "a field's type must be trivially copyable or nothrow move assignable");
    static_assert((std::is_nothrow_destructible_v<typename Fields::type> && ...),
                  "a field's type must be nothrow destructible");
};

} // namespace floormat

namespace floormat::detail::component {

template<typename Component>
using field_list_of = decltype(component_fields<Component>::fields());

template<typename Fields> struct field_list_traits;

template<> struct field_list_traits<no_fields>
{
    static constexpr u32 field_count = 0;
};

template<auto COUNT, typename... Fields> struct field_list_traits<field_list<COUNT, Fields...>>
{
    using field_enum = decltype(COUNT);
    static constexpr u32 field_count = sizeof...(Fields);
};

template<auto Enumerator, typename Fields> struct stored_type;

template<auto Enumerator, auto COUNT, typename... Fields>
struct stored_type<Enumerator, field_list<COUNT, Fields...>>
{
    static_assert((usize)Enumerator < sizeof...(Fields), "the enumerator names no field");
    using type = nth_type<(u32)Enumerator, typename Fields::type...>::type;
};

template<typename E, typename Component>
consteval bool has_field_enum()
{
    using Fields = field_list_of<Component>;
    if constexpr (std::is_same_v<Fields, no_fields>)
        return false;
    else
        return std::is_same_v<typename field_list_traits<Fields>::field_enum, E>;
}

template<typename E, typename... Components>
consteval u32 index_of_component_with_field_enum()
{
    constexpr u32 num_components_with_enum = ((u32)has_field_enum<E, Components>() + ... + 0);
    static_assert(num_components_with_enum != 0, "no listed component has the enumerator's field enum");
    static_assert(num_components_with_enum < 2, "two listed components share one field enum");
    constexpr bool has_enum[] = { has_field_enum<E, Components>()..., false };
    u32 i = 0;
    while (i < sizeof...(Components) && !has_enum[i])
        i++;
    return i;
}

template<auto Enumerator, typename... Components>
consteval u32 field_index()
{
    constexpr u32 component = index_of_component_with_field_enum<decltype(Enumerator), Components...>();
    constexpr u32 field_counts[] = { field_list_traits<field_list_of<Components>>::field_count..., 0 };
    static_assert((u32)Enumerator < field_counts[component], "the enumerator names no field");
    u32 index = (u32)Enumerator;
    for (u32 i = 0; i < component; i++)
        index += field_counts[i];
    return index;
}

template<auto Enumerator, typename... Components>
struct field_type
{
    using component = nth_type<index_of_component_with_field_enum<decltype(Enumerator), Components...>(),
                               Components...>::type;
    using type = stored_type<Enumerator, field_list_of<component>>::type;
};

} // namespace floormat::detail::component

namespace floormat {

template<typename Component>
using field_enum_of = detail::component::field_list_traits<
    detail::component::field_list_of<Component>>::field_enum;

template<typename Component>
constexpr inline u32 field_count =
    detail::component::field_list_traits<detail::component::field_list_of<Component>>::field_count;

// Exactly one of Components must have the enumerator's field enum.
template<auto Enumerator, typename... Components>
using field_type = detail::component::field_type<Enumerator, Components...>::type;

// Fields are numbered component after component, in the order Components lists them.
template<auto Enumerator, typename... Components>
constexpr inline u32 field_index = detail::component::field_index<Enumerator, Components...>();

} // namespace floormat
