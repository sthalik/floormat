#pragma once
#include "handle-pool.hpp"
#include "handle-page.hpp"
#include "compat/move.hpp"
#include <cr/GrowableArray.h>

namespace floormat::impl_handle {

template<typename Obj, uint32_t PageSize>
template<typename... Xs>
requires requires (Xs&&... xs) {
    Obj{forward<Xs>(xs)...};
}
Handle<Obj, PageSize> Pool<Obj, PageSize>::make_object(Xs&&... xs)
{
    auto ret = std::shared_ptr<T>(new Obj{forward<Xs>(xs)...});
    do_make_object(std::static_pointer_cast<object>(ret));
    return ret;
}

template<typename Obj, uint32_t PageSize>
Pointer<Pool<Obj, PageSize>>
Pool<Obj, PageSize>::make_pool()
{
    auto pool = Pointer<Pool<Obj, PageSize>>{InPlace};
    arrayReserve(pool->pages, 16);
    return pool;
}

template<typename Obj, uint32_t PageSize>
Page<Obj, PageSize>&
Pool<Obj, PageSize>::find_page()
{
    auto& P = *pool;
    auto sz = P.pages.size();
    for (auto i = 0uz; i < sz; i++)
    {
        auto& p = *P.pages.data()[i];
        if (!p.is_full())
            return p;
    }
    arrayAppend(P.pages, InPlace, InPlace, P.next_page_offset);
    P.next_page_offset += PageSize;
    return P.pages.back();
}

} // namespace floormat::impl_handle
