#pragma once
#include "handle-fwd.hpp"
#include "compat/defs.hpp"
#include <array>
#include <cr/BitArray.h>

namespace floormat::impl_handle {

template<typename Obj, uint32_t PageSize>
struct Item
{
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Item);
    Item();
    ~Item() noexcept;

    union { char empty = {}; Obj object; };
    Handle<Obj, PageSize> handle;
    uint32_t next;
};

template<typename Obj, uint32_t PageSize>
class Page
{
    friend struct Handle<Obj, PageSize>;

    std::array<Item<Obj, PageSize>, PageSize> storage;
    BitArray used_map; // todo replace with a rewrite of std::bitset
    uint32_t start_index;
    uint32_t used_count;
    uint32_t first_free;
    bool locked;

    static void do_deallocate(Item<Obj, PageSize>& item);

public:
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Page);

    explicit Page(uint32_t start_index);
    ~Page() noexcept;

    template<typename... Xs>
    requires requires (Xs&&... xs) {
        Obj{forward<Xs>(xs)...};
    }
    [[nodiscard]] Item<Obj, PageSize>& allocate(Xs&&... xs);
    void deallocate(Handle<Obj, PageSize> obj);
    void deallocate_all();
    bool is_empty();
    bool is_full();
    uint32_t use_count() const;
};

} // namespace floormat::impl_handle
