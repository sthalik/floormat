#include "borrowed-ptr.inl"
#include "compat/assert.hpp"

namespace floormat::detail_bptr {

void control_block::decrement(control_block*& blk) noexcept
{
    auto c = --blk->_count;
    fm_bptr_assert(c != (uint32_t)-1);
    if (c == 0)
    {
        delete blk->_ptr;
        delete blk;
    }
    blk = nullptr;
    //blk = (control_block*)-1;
}

} // namespace floormat::detail_bptr

namespace floormat {

bptr_base::~bptr_base() noexcept = default;
bptr_base::bptr_base() noexcept = default;
bptr_base::bptr_base(const bptr_base&) noexcept = default;
bptr_base::bptr_base(bptr_base&&) noexcept = default;
bptr_base& bptr_base::operator=(const bptr_base&) noexcept = default;
bptr_base& bptr_base::operator=(bptr_base&&) noexcept = default;

} // namespace floormat
