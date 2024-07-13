#include "app.hpp"
//#include "src/handle.inl"
#include "src/handle.hpp"
#include "src/handle-page.inl"
//#include "src/handle-pool.inl"

namespace floormat {
namespace {

constexpr uint32_t start_index = 1024, page_size = 128;

struct Foo
{
    int value = 0;
    ~Foo() { value = 0; }
};

using Page = impl_handle::Page<Foo, page_size>;
using Handle = impl_handle::Handle<Foo, page_size>;

} // namespace
} // namespace floormat

namespace floormat::impl_handle {

template struct Handle<Foo, page_size>;
template struct Item<Foo, page_size>; // NOLINT(*-pro-type-member-init)
template class Page<Foo, page_size>;
//template class Pool<Foo, page_size>;

} // namespace floormat::impl_handle


namespace floormat::Test {

namespace {

void test_page1()
{
    Page page{start_index};
    auto& item0 = page.allocate(1);
    fm_assert(page.use_count() == 1);
    auto handle0 = item0.handle;
    fm_assert(item0.handle == Handle{start_index, 0});
    fm_assert(item0.object.value == 1);
    page.deallocate(item0.handle);
    fm_assert(page.use_count() == 0);

    auto& item1 = page.allocate(2);
    fm_assert(page.use_count() == 1);
    auto& item2 = page.allocate(3);
    fm_assert(page.use_count() == 2);
    fm_assert(item1.object.value == 2);
    fm_assert(item2.object.value == 3);
    fm_assert(item1.handle.index == item0.handle.index || item2.handle.index == item0.handle.index);
    fm_assert(item1.handle.index != item2.handle.index);
    fm_assert(item1.handle.counter != item2.handle.counter);
    fm_assert(int{item1.handle.counter != handle0.counter} + int{item2.handle.counter != handle0.counter} == 1);
    page.deallocate(item2.handle);
    page.deallocate(item1.handle);
    fm_assert(item0.object.value == 0);
    fm_assert(item1.object.value == 0);
    fm_assert(item2.object.value == 0);
    fm_assert(page.use_count() == 0);
}
} // namespace


void test_handle()
{
    test_page1();
}

} // namespace floormat::Test
