#include "borrowed-ptr.inl"
#include "compat/assert.hpp"

namespace floormat::detail_borrowed_ptr {

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdelete-abstract-non-virtual-dtor"
#elif defined __GNUG__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#elif defined _MSC_VER
#pragma warning(push)
#pragma warning(disable : 5205)
#endif
void control_block::decrement(control_block*& blk) noexcept
{
    fm_bptr_assert(blk);
    auto c = --blk->_count;
    fm_bptr_assert(c != (uint32_t)-1);
    if (c == 0)
    {
        blk->free_ptr();
        delete blk;
    }
    //blk = nullptr;
    blk = (control_block*)-1;
}

#ifdef __clang__
#pragma clang diagnostic pop
#elif defined __GNUG__
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning(pop)
#endif

} // namespace floormat::detail_borrowed_ptr
