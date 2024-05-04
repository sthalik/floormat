#include "borrowed-ptr.inl"
#include "compat/assert.hpp"

namespace floormat::detail_borrowed_ptr {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-abstract-non-virtual-dtor"
#elif defined __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif
void control_block_::decrement(control_block_*& blk) noexcept
{
    fm_bptr_assert(blk);
    auto c = --blk->_count;
    fm_bptr_assert(c != (uint32_t)-1);
    if (c == 0)
    {
        blk->free_ptr();
        delete blk;
    }
    blk = nullptr;
}

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined __GNUG__
#pragma GCC diagnostic pop
#endif

} // namespace floormat::detail_borrowed_ptr
