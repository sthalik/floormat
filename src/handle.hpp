#pragma once
#include "handle-fwd.hpp"
#include "compat/assert.hpp"

namespace floormat::impl_handle {

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

} // namespace floormat::impl_handle

namespace floormat {

} // namespace floormat
