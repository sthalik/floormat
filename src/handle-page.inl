#pragma once
#include "handle-page.hpp"
#include "compat/assert.hpp"
#include "compat/pretty-function.hpp"

namespace floormat::impl_handle {

template<typename Obj, uint32_t PageSize>
Item<Obj, PageSize>::Item() = default;

template<typename Obj, uint32_t PageSize>
Item<Obj, PageSize>::~Item() noexcept
{}

template<typename Obj, uint32_t PageSize>
void Page<Obj, PageSize>::do_deallocate(Item<Obj, PageSize>& item)
{
    item.object.~Obj();
}

template<typename Obj, uint32_t PageSize>
Page<Obj, PageSize>::~Page() noexcept
{
    fm_assert(used_count == 0);
}

template<typename Obj, uint32_t PageSize>
Page<Obj, PageSize>::Page(uint32_t start_index):
    used_map{ValueInit, PageSize},
    start_index{start_index},
    used_count{0},
    first_free{start_index},
    locked{false}
{
    fm_assert(start_index + PageSize > start_index);
    auto val = start_index;
    for (auto i = 0u; i < PageSize; i++)
    {
        auto& o = storage.data()[i];
        o.handle = {val++, 0};
        o.next = val;
    }
}

template<typename Obj, uint32_t PageSize>
template<typename... Xs>
requires requires (Xs&&... xs) {
    Obj{forward<Xs>(xs)...};
}
Item<Obj, PageSize>& Page<Obj, PageSize>::allocate(Xs&&... xs)
{
    fm_assert(!locked);
    locked = true;
    fm_assert(used_count < PageSize);
    auto first_freeʹ = first_free - start_index;
    fm_debug_assert(first_freeʹ < PageSize);
    fm_debug_assert(!used_map[first_freeʹ]);
    auto& item = storage[first_freeʹ];
    first_free = item.next;
    used_count++;
    used_map.set(first_freeʹ, true);
    new (&item.object) Obj{ forward<Xs>(xs)... };
    locked = false;
    return item;
}

template<typename Obj, uint32_t PageSize>
void Page<Obj, PageSize>::deallocate(Handle<Obj, PageSize> obj)
{
    fm_assert(!locked);
    locked = true;
    auto index = obj.index - start_index;
    fm_debug_assert(index < PageSize);
    fm_assert(used_map[index]);
    fm_debug_assert(used_count > 0);
    fm_debug_assert(first_free != (uint32_t)-1);
    auto& item = storage[index];
    auto ctr = item.handle.counter++;
    if (ctr == (uint32_t)-1) [[unlikely]]
        fm_debug("counter overflowed: %s", FM_PRETTY_FUNCTION);
    fm_assert(obj.counter == ctr);
    item.next = first_free;
    first_free = obj.index;
    used_count--;
    used_map.set(index, false);
    do_deallocate(item);
    locked = false;
}

template<typename Obj, uint32_t PageSize>
void Page<Obj, PageSize>::deallocate_all()
{
    fm_assert(!locked);
    locked = true;
    for (auto i = 0u; i < PageSize; i++)
        if (used_map[i])
            do_deallocate(storage[i]);
    auto val = start_index;
    for (auto i = 0u; i < PageSize; i++)
    {
        auto& o = storage.data()[i];
        bool used = used_map[i];
        uint32_t new_ctr = used ? 1 : 0;
        o.handle = {val++, o.handle.counter + new_ctr};
        o.next = val;
    }
    first_free = start_index;
    used_count = 0;
    used_map = BitArray{ValueInit, PageSize};
    locked = false;
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

template<typename Obj, uint32_t PageSize>
uint32_t Page<Obj, PageSize>::use_count() const
{
    return used_count;
}

} // namespace floormat::impl_handle
