#pragma once
#include "component-table.hpp"
#include "compat/assert.hpp"
#include "compat/lower-bound.hpp"
#include <cr/GrowableArray.h>
#include <cr/Optional.h>

namespace floormat {

template<typename... Components>
u32 component_table<Components...>::lower_bound(object_id id) const noexcept
{
    const object_id* const first = std::get<0>(_arrays).data();
    const auto it = floormat::lower_bound(first, first + size(), id, [](object_id a, object_id b) { return a < b; });
    return (u32)(it - first);
}

template<typename... Components>
void component_table<Components...>::make_room_for_row()
{
    const u32 n = size();
    fm_assert(n < u32(-1) - 1);
    // Corrade picks a grown capacity from the array's size in bytes, so each array would get a different one.
    if (n == arrayCapacity(std::get<0>(_arrays)))
    {
        const u64 capacity = n < 16 ? 16 : n + u64{n / 2};
        reserve(capacity < u32(-1) ? (u32)capacity : u32(-1));
    }
}

template<typename... Components>
u32 component_table<Components...>::insert_row(object_id id)
{
    const u32 row = lower_bound(id);
    fm_assert(row == size() || ids()[row] != id);
    make_room_for_row();
    std::apply([=](Array<object_id>& id_array, auto&... field_arrays) {
        arrayInsert(id_array, row, InPlaceInit, id);
        (arrayInsert(field_arrays, row, ValueInit, 1), ...);
    }, _arrays);
    return row;
}

template<typename... Components>
Optional<u32> component_table<Components...>::find_row(object_id id) const noexcept
{
    const u32 row = lower_bound(id);
    if (row < size() && ids()[row] == id)
        return row;
    else
        return {};
}

template<typename... Components>
void component_table<Components...>::erase_row(u32 row)
{
    fm_assert(row < size());
    std::apply([=](auto&... arrays) { (arrayRemove(arrays, row), ...); }, _arrays);
}

template<typename... Components>
u32 component_table<Components...>::move_row_to(component_table& dst, u32 row)
{
    fm_assert(&dst != this);
    fm_assert(row < size());
    const object_id id = ids()[row];
    const u32 dst_row = dst.lower_bound(id);
    fm_assert(dst_row == dst.size() || dst.ids()[dst_row] != id);
    dst.make_room_for_row();
    const auto move_element = [=](auto& to, auto& from) {
        arrayInsert(to, dst_row, InPlaceInit, move(from[row]));
        arrayRemove(from, row);
    };
    [&]<usize... Is>(std::index_sequence<Is...>) {
        (move_element(std::get<Is>(dst._arrays), std::get<Is>(_arrays)), ...);
    }(std::make_index_sequence<std::tuple_size_v<arrays_tuple>>{});
    return dst_row;
}

template<typename... Components>
void component_table<Components...>::reserve(u32 capacity)
{
    std::apply([=](auto&... arrays) { (arrayReserve(arrays, capacity), ...); }, _arrays);
}

template<typename... Components>
void component_table<Components...>::clear()
{
    std::apply([](auto&... arrays) { (arrayClear(arrays), ...); }, _arrays);
}

} // namespace floormat
