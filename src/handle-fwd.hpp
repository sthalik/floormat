#pragma once

namespace floormat::Handle {

template<typename Obj, uint32_t PageSize> struct Item;

template<typename OBJ, uint32_t PAGE_SIZE>
struct Handle
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

} // namespace floormat::Handle

namespace floormat {

template<typename Obj, uint32_t PageSize>
using handle = struct Handle::Handle<Obj, PageSize>;

} // namespace floormat
