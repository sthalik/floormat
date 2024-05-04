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

int A_total = 0, A_alive = 0; // NOLINT

struct A
{
    int val, serial;
    explicit A(int val) : val{val}, serial{++A_total} { ++A_alive; }
    ~A() noexcept { --A_alive; fm_assert(A_alive >= 0); }
};

void test1()
{
    A_total = 0; A_alive = 0;

    auto p1 = bptr<A>{InPlace, 1};
    fm_assert(p1.use_count() == 1);
    fm_assert(p1.get());
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p1 = nullptr;
    fm_assert(p1.use_count() == 0);
    fm_assert(!p1.get());
    fm_assert(A_total == 1);
    fm_assert(A_alive == 0);
}

void test2()
{
    A_total = 0; A_alive = 0;

    auto p1 = bptr<A>{InPlace, 2};
    auto p2 = p1;

    fm_assert(p1.get());
    fm_assert(p1.get() == p2.get());
    fm_assert(p1->val == 2);
    fm_assert(p1->serial == 1);
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p1 = nullptr;
    fm_assert(!p1.get());
    fm_assert(p2.get());
    fm_assert(p2->val == 2);
    fm_assert(p2->serial == 1);
    fm_assert(A_total == 1);
    fm_assert(A_alive == 1);

    p2 = nullptr;
    fm_assert(!p1.get());
    fm_assert(!p2.get());
    fm_assert(A_total == 1);
    fm_assert(A_alive == 0);
}

} // namespace

void test_app::test_bptr()
{
    test1();
    test2();
}

} // namespace floormat
