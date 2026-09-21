#include "app.hpp"
#include "compat/array-size.hpp"
#include <mg/Vector4.h>
#include <mg/Color.h>

namespace floormat::Test {

namespace {

struct Foo
{
    static constexpr std::array<int, 11> Array_1 = {};
    static constexpr const void* Array_2[22] = {};

    std::array<int, 33> array_3;
    int array_4[55] = {};
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

    fm_assert(static_array_size<decltype(Foo::array_4)> == 55);
    fm_assert(array_size(&Foo::array_4) == 55);
    fm_assert(array_size(Foo{}.array_4) == 55);

    return true;
}

constexpr bool test_array_size_magnum()
{
    fm_assert(static_array_size<Math::Vector<7, float>> == 7);
    fm_assert(static_array_size<Vector2i> == 2);
    fm_assert(static_array_size<Vector3ub> == 3);
    fm_assert(static_array_size<Vector4> == 4);
    fm_assert(static_array_size<Color3> == 3);
    fm_assert(static_array_size<Color4ub> == 4);

    fm_assert(array_size(Vector3i{}) == 3);
    fm_assert(array_size(Color4{}) == 4);

    return true;
}

} // namespace

void test_util()
{
    static_assert(test_array_size());
    static_assert(test_array_size_magnum());
}

} // namespace floormat::Test
