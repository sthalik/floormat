#pragma once
#include "handle.hpp"
//#include "handle-pool.inl"
#include "compat/assert.hpp"
#include <cr/Pointer.h>

namespace floormat::Handle {

template<typename Obj, uint32_t PageSize> class Pool;

template<typename Obj, uint32_t PageSize>
bool Handle<Obj, PageSize>::operator==(const Handle& other) const noexcept
{
    bool ret = index == other.index;
    fm_debug_assert(!ret || counter == other.counter);
    return ret;
}

template<typename Obj, uint32_t PageSize>
Handle<Obj, PageSize>::operator bool() const noexcept
{
    return index != (uint32_t)-1;
}

template<typename Obj, uint32_t PageSize>
const Obj& Handle<Obj, PageSize>::get() const
{
    return get_from_pool(index, counter).object;
}

template<typename Obj, uint32_t PageSize>
Obj& Handle<Obj, PageSize>::get()
{
    return get_from_pool(index, counter).object;
}

template<typename Obj, uint32_t PageSize>
Item<Obj, PageSize>& Handle<Obj, PageSize>::get_from_pool(uint32_t index, uint32_t counter)
{
    auto page_idx = index / PageSize;
    auto obj_idx = index % PageSize;
    fm_assert(Pool<Obj, PageSize>::pages.size() < page_idx);
    auto& page = Pool<Obj, PageSize>::pages.data()[page_idx];
    auto& item = page->storage[obj_idx];
    fm_debug_assert(page.used_map[obj_idx]);
    fm_assert(item.handle.counter == counter);
    fm_debug_assert(item.handle == index + page.start_index);
    return item.object;
}

} // namespace floormat::Handle

namespace floormat {



} // namespace floormat
