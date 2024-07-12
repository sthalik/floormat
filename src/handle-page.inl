#pragma once
#include "handle-page.hpp"
#include "compat/assert.hpp"
#include "compat/pretty-function.hpp"

namespace floormat::impl_handle {

template<typename Obj, uint32_t PageSize>
Page<Obj, PageSize>::~Page() noexcept
{
    fm_assert(used_count == 0);
}

template<typename Obj, uint32_t PageSize>
Page<Obj, PageSize>::Page(uint32_t start_index):
    used_map{ValueInit, PageSize},
    start_index{start_index},
    first_free{start_index}
{
    fm_assert(size_t{start_index} + size_t{PageSize} <= size_t{1u<<31} && start_index + PageSize > start_index);
    auto val = start_index;
    for (auto i = 0u; i < PageSize; i++)
    {
        auto& o = storage.data()[i];
        o.handle = {val++, 0};
        o.next = val;
    }
}

template<typename Obj, uint32_t PageSize>
Item<Obj, PageSize>& Page<Obj, PageSize>::allocate()
{
    fm_assert(used_count < PageSize);
    auto first_freeʹ = first_free - start_index;
    fm_debug_assert(first_freeʹ < PageSize);
    fm_debug_assert(!used_map[first_freeʹ]);
    auto& item = storage[first_freeʹ];
    first_free = item.next;
    used_count++;
    used_map.set(first_freeʹ, true);
    return item;
}

template<typename Obj, uint32_t PageSize>
void Page<Obj, PageSize>::deallocate(Handle<Obj, PageSize> obj)
{
    auto index = obj.index - start_index;
    fm_debug_assert(index < PageSize);
    fm_assert(used_map[index]);
    fm_debug_assert(used_count > 0);
    fm_debug_assert(first_free != (uint32_t)-1);
    auto& item = storage[index];
    auto ctr = item.handle.counter++;
    if (ctr == (uint32_t)-1) [[unlikely]]
        Debug{} << "counter" << FM_PRETTY_FUNCTION << "overflowed";
    fm_assert(obj.counter == ctr);
    item.next = first_free;
    used_count--;
    used_map.set(index, false);
    first_free = obj.index;
}

template<typename Obj, uint32_t PageSize>
bool Page<Obj, PageSize>::is_empty()
{
    return used_count == 0;
}

template<typename Obj, uint32_t PageSize>
bool Page<Obj, PageSize>::is_full()
{
    return used_count == PageSize;
}

} // namespace floormat::impl_handle
