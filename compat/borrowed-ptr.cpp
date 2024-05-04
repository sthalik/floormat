#include "borrowed-ptr.inl"
#include "compat/assert.hpp"

namespace floormat::detail_borrowed_ptr {

control_block_::control_block_(void* ptr) noexcept: _ptr{ptr}, _count{1}
{
    fm_bptr_assert(ptr);
}

void control_block_::incr() noexcept
{
    auto val = ++_count;
    (void)val;
    fm_bptr_assert(val > 1);
}

void control_block_::decr() noexcept
{
    auto val = --_count;
    fm_bptr_assert(val != (uint32_t)-1);
    if (val == 0)
    {
        free();
        _ptr = nullptr;
    }
}

control_block_::~control_block_() noexcept { decr(); }
uint32_t control_block_::count() const noexcept { return _count; }

} // namespace floormat::detail_borrowed_ptr

namespace {
struct Foo {};
struct Bar : Foo {};
struct Baz {};
} // namespace

namespace floormat {

template struct detail_borrowed_ptr::control_block<Foo>;
template class bptr<Foo>;
template bptr<Bar> static_pointer_cast(const bptr<Foo>&) noexcept;

} // namespace floormat
