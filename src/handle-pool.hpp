#pragma once
#include "compat/defs.hpp"
#include <cr/Array.h>
#include <cr/Pointer.h>

namespace floormat::impl_handle {

template<typename Obj, uint32_t PageSize>
class Pool
{
    friend class impl_handle<Obj, PageSize>;

    static Pointer<Pool<Obj, PageSize>> make_pool();
    static Page<Obj, PageSize>& find_page();

    Array<Pointer<Page<Obj, PageSize>>> pages;
    size_t next_page_offset = 0;

    static Pointer<Pool<Obj, PageSize>> pool = make_pool();

public:
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Pool);

    explicit Pool();
    ~Pool() noexcept;

    template<typename... Xs>
    requires requires (Xs&&... xs) {
        Obj{forward<Xs>(xs)...};
    }
    static impl_handle<Obj, PageSize> make_object(Xs&&... xs);
};

} // namespace floormat::impl_handle
