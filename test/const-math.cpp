#include "app.hpp"
#include "compat/assert.hpp"
#include <type_traits>
#include <Magnum/Math/Vector.h>
#include <Magnum/Math/Vector2.h>
#include <Magnum/Math/Vector3.h>
#include <Magnum/Math/Vector4.h>

#if defined CORRADE_CONSTEVAL || defined MAGNUM_CONSTEXPR14

#ifdef __GNUG__
#   pragma GCC diagnostic push
#   pragma GCC diagnostic ignored "-Wfloat-equal"
#endif

using namespace Magnum;
using Magnum::Math::Vector;

template<typename vec, typename T>
static constexpr void test_float2()
{
    const vec a{(T)1, (T)2}, b{(T)2, (T)3};

    fm_assert(a[0] == (T)1 && a[1] == (T)2);
    fm_assert(a + b == vec{(T)3,  (T)5});
    fm_assert(a - b == vec{(T)-1, (T)-1});
    fm_assert(a * b == vec{(T)2,  (T)6});
    fm_assert(b / a == vec{(T)2,  (T)1.5});
    fm_assert(b.product() == (T)6);
    fm_assert(b.sum() == (T)5);
}

template<typename ivec>
static constexpr void test_int()
{
    using I = typename ivec::Type;
    constexpr auto vec = [](auto x, auto y) { return ivec{(I)x, (I)y}; };
    const auto a = vec(3, 5), b = vec(11, 7);

    fm_assert(a[0] == 3 && a[1] == 5);
    fm_assert(a + b == vec(14,12));
    fm_assert(b - a == vec(8, 2));
    fm_assert(b % a == vec(2, 2));
    fm_assert(b / a == vec(3, 1));
    fm_assert(a.product() == 15);
    fm_assert(a.sum() == 8);
}

static constexpr void* compile_tests()
{
    test_float2<Vector<2, float>, float>();
    test_float2<Vector<2, double>, double>();
    test_float2<Vector2, float>();

    test_int<Vector<2, int>>();
    test_int<Vector<2, unsigned>>();
    test_int<Vector<2, char>>();

    return nullptr;
}

namespace floormat {

bool test_app::test_const_math()
{
    static_assert(compile_tests() == nullptr);
    return true;
}

} // namespace floormat

#ifdef __GNUG__
#   pragma GCC diagnostic pop
#endif
#else
namespace floormat { bool floormat::test_const_math() { return true; } }
#endif
