#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/borrowed-ptr.inl"

namespace floormat {

namespace {
struct Foo {};
struct Bar : Foo {};
struct Baz {};
} // namespace

// NOLINTBEGIN(*-use-anonymous-namespace)

template struct detail_borrowed_ptr::control_block_impl<Foo>;
template struct detail_borrowed_ptr::control_block_impl<Bar>;
template struct detail_borrowed_ptr::control_block_impl<Baz>;

template bool operator==(const bptr<Foo>&, const bptr<Foo>&) noexcept;
template bool operator==(const bptr<Bar>&, const bptr<Bar>&) noexcept;
template bool operator==(const bptr<Baz>&, const bptr<Baz>&) noexcept;

template class bptr<Foo>;
template class bptr<Bar>;
template class bptr<Baz>;

template bptr<Foo> static_pointer_cast(const bptr<Foo>&) noexcept;
template bptr<Bar> static_pointer_cast(const bptr<Bar>&) noexcept;
template bptr<Baz> static_pointer_cast(const bptr<Baz>&) noexcept;

template bptr<Bar> static_pointer_cast(const bptr<Foo>&) noexcept;
template bptr<Foo> static_pointer_cast(const bptr<Bar>&) noexcept;

//template bptr<Baz> static_pointer_cast(const bptr<Bar>&) noexcept; // must fail
//template bptr<Foo> static_pointer_cast(const bptr<Baz>&) noexcept; // must fail
//template bptr<Bar> static_pointer_cast(const bptr<Baz>&) noexcept; // must fail

// NOLINTEND(*-use-anonymous-namespace)

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
