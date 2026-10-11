#include "borrowed-ptr.inl"
#include "assert.hpp"

namespace floormat::detail_bptr {

template struct control_block<non_atomic_refcount>;

} // namespace floormat::detail_bptr

namespace floormat::bptr_policy {

namespace { thread_local char thread_tag; } // NOLINT(*-avoid-non-const-global-variables)

void thread_check(thread_check_state& s) noexcept
{
    const void* tag = &thread_tag;
    if (!s.owner)
        s.owner = tag;
    else
        fm_assert(s.owner == tag);
}

void no_stats::block_allocated() noexcept {}
void no_stats::block_deallocated() noexcept {}
void no_stats::object_constructed() noexcept {}
void no_stats::object_disposed() noexcept {}
void no_stats::copied() noexcept {}
void no_stats::released() noexcept {}

} // namespace floormat::bptr_policy

namespace floormat {

bptr_base::~bptr_base() noexcept = default;
bptr_base::bptr_base() noexcept = default;
bptr_base::bptr_base(const bptr_base&) noexcept = default;
bptr_base::bptr_base(bptr_base&&) noexcept = default;
bptr_base& bptr_base::operator=(const bptr_base&) noexcept = default;
bptr_base& bptr_base::operator=(bptr_base&&) noexcept = default;

} // namespace floormat
