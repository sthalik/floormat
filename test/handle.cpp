#include "app.hpp"
//#include "src/handle.inl"
#include "src/handle.hpp"
#include "src/handle-page.inl"
//#include "src/handle-pool.inl"

namespace floormat {
namespace {

constexpr uint32_t start_index = 1024, page_size = 128;
struct Foo { int value; };
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
    auto& item = page.allocate();
    fm_assert(item.handle == Handle{start_index});
    page.deallocate(item.handle);
}
} // namespace


void test_handle()
{
    test_page1();
}

} // namespace floormat::Test
