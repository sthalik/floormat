#pragma once
#include "handle-fwd.hpp"
#include "compat/defs.hpp"
#include <array>
#include <cr/BitArray.h>

namespace floormat::Handle {

template<typename Obj, uint32_t PageSize>
struct Item
{
    union { Obj object; };
    Handle<Obj, PageSize> handle;
    uint32_t next;
};

template<typename Obj, uint32_t PageSize>
class Page
{
    friend class Handle<Obj, PageSize>;

    std::array<Item<Obj, PageSize>, PageSize> storage;
    BitArray used_map; // todo replace with a rewrite of std::bitset
    uint32_t start_index;
    uint32_t used_count = 0;
    uint32_t first_free = (uint32_t)-1;

public:
    fm_DECLARE_DELETED_COPY_MOVE_ASSIGNMENTS(Page);

    explicit Page(uint32_t start_index);
    ~Page() noexcept;

    [[nodiscard]] Item<Obj, PageSize>& allocate();
    void deallocate(Handle<Obj, PageSize> obj);
    bool is_empty();
    bool is_full();
};

} // namespace floormat::Handle
