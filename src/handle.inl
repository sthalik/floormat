#pragma once
#include "handle.hpp"
//#include "handle-pool.inl"
#include "compat/assert.hpp"

namespace floormat::impl_handle {

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

} // namespace floormat::impl_handle

namespace floormat {



} // namespace floormat
