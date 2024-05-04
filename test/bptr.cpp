#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"

namespace floormat {

namespace { struct Foo {}; struct Bar : Foo {}; struct Baz {}; }
template struct detail_borrowed_ptr::control_block_impl<Foo>;
template class bptr<Foo>;
template bptr<Bar> static_pointer_cast(const bptr<Foo>&) noexcept;

namespace {

void test1()
{
}

} // namespace

void test_app::test_bptr()
{
    test1();
}


} // namespace floormat
