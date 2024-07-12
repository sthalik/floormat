#pragma once

namespace floormat::impl_handle {

template<typename OBJ, uint32_t PAGE_SIZE> struct Handle;
template<typename Obj, uint32_t PageSize> struct Item;
template<typename Obj, uint32_t PageSize> class Page;
template<typename Obj, uint32_t PageSize> class Pool;

template<typename OBJ, uint32_t PAGE_SIZE>
struct Handle final
{
    uint32_t index = (uint32_t)-1;
    uint32_t counter = 0;

#if 0
    using Obj = OBJ;
    static constexpr auto PageSize = PAGE_SIZE;
#endif

    const OBJ& get() const;
    OBJ& get();

    bool operator==(const Handle& other) const noexcept;

    explicit operator bool() const noexcept;

private:
    static Item<OBJ, PAGE_SIZE>& get_from_pool(uint32_t index, uint32_t counter);
};

} // namespace floormat::impl_handle

namespace floormat {

template<typename Obj, uint32_t PageSize>
using handle = struct impl_handle::Handle<Obj, PageSize>;

} // namespace floormat
