#include "borrowed-ptr.inl"

namespace floormat::detail_bptr {

void control_block::decrement(control_block*& blk) noexcept
{
    auto c2 = --blk->_hard_count;
    fm_bptr_assert(c2 != (uint32_t)-1);
    if (c2 == 0)
    {
        delete blk->_ptr;
        blk->_ptr = nullptr;
#ifdef FM_NO_WEAK_BPTR
        delete blk;
#endif
    }
#ifndef FM_NO_WEAK_BPTR
    auto c = --blk->_soft_count;
    fm_bptr_assert(c != (uint32_t)-1);
    if (c == 0)
    {
        fm_bptr_assert(!blk->_ptr);
        delete blk;
    }
#endif
    blk = nullptr;
}

#ifndef FM_NO_WEAK_BPTR
void control_block::weak_decrement(control_block*& blk) noexcept
{
    if (!blk)
        return;
    fm_bptr_assert(blk->_hard_count < blk->_soft_count);
    auto c = --blk->_soft_count;
    //fm_bptr_assert(c != (uint32_t)-1);
    if (c == 0)
    {
        fm_bptr_assert(!blk->_ptr);
        delete blk;
    }
    blk = nullptr;
}
#endif

} // namespace floormat::detail_bptr

namespace floormat {

bptr_base::~bptr_base() noexcept = default;
bptr_base::bptr_base() noexcept = default;
bptr_base::bptr_base(const bptr_base&) noexcept = default;
bptr_base::bptr_base(bptr_base&&) noexcept = default;
bptr_base& bptr_base::operator=(const bptr_base&) noexcept = default;
bptr_base& bptr_base::operator=(bptr_base&&) noexcept = default;

} // namespace floormat
