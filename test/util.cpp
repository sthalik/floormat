#include "app.hpp"
#include "compat/array-size.hpp"

namespace floormat::Test {

namespace {

struct Foo
{
    static constexpr std::array<int, 11> Array_1 = {};
    static constexpr const void* Array_2[22] = {};

    std::array<int, 33> array_3;
};

constexpr bool test_array_size()
{
    fm_assert(static_array_size<decltype(Foo::Array_1)> == 11);
    fm_assert(array_size(Foo::Array_1) == 11);

    fm_assert(static_array_size<decltype(Foo::Array_2)> == 22);
    fm_assert(array_size(&Foo::Array_2) == 22);

    fm_assert(static_array_size<decltype(Foo{}.array_3)> == 33);
    fm_assert(array_size(Foo{}.array_3) == 33);
    fm_assert(array_size(&Foo::array_3) == 33);

    fm_assert(static_array_size<const int(&)[44]> == 44);

    return true;
}

} // namespace

void test_util()
{
    static_assert(test_array_size());
}

} // namespace floormat::Test
