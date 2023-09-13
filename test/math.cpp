#include "app.hpp"
#include "compat/math.hpp"

namespace floormat {

namespace {

constexpr bool test_double_sqrt()
{
    using F = double;
    constexpr auto eps = F(1e-11);

    static_assert(math::abs(math::sqrt((F)3)    - (F)1.73205080757) < eps);
    return true;
}

template<typename F>
bool test_sqrt()
{
    constexpr auto eps = F(1e-11);
    constexpr auto test = [](double x)
    {
        auto x_ = (F)x;
        auto y1 = math::sqrt(x_);
        auto y2 = std::sqrt(x_);
        return math::abs(y1 - y2) < eps;
    };

    static_assert(math::abs(math::sqrt((F)0)    - (F)0) < eps);
    static_assert(math::abs(math::sqrt((F)0.5)  - (F)0.70710678118) < eps);
    static_assert(math::abs(math::sqrt((F)1e-8) - (F)0.0001)        < eps);
    static_assert(math::abs(math::sqrt((F)2)    - (F)1.41421356237) < eps);
    static_assert(math::abs(math::sqrt((F)3)    - (F)1.73205080757) < (F)1e-6);

    static_assert(math::sqrt((F)0)  == (F)0);
    static_assert(math::sqrt((F)1)  == (F)1);
    static_assert(math::sqrt((F)4)  == (F)2);
    static_assert(math::sqrt((F)9)  == (F)3);
    static_assert(math::sqrt((F)36) == (F)6);

    fm_assert(test(0));
    fm_assert(test(0.5));
    fm_assert(test(1.5));
    fm_assert(test(2));
    fm_assert(test(3));
    fm_assert(test(0));
    fm_assert(test(42));
    fm_assert(test(41.5));
    fm_assert(test(1e-8));
    fm_assert(test(1e8));
    fm_assert(test(1.23456789));
    fm_assert(test(1e10 + 1.23456789));
    fm_assert(test(531610));
    fm_assert(test(1e10));
    fm_assert(test(1 << 20));

    return true;
}

template<typename F>
constexpr bool test_floor()
{
    fm_assert(math::floor((F)-1.5) == -2);
    fm_assert(math::floor((F)0) == 0);
    fm_assert(math::floor((F)1) == 1);
    fm_assert(math::floor((F)-1) == -1);
    fm_assert(math::floor((F)-2) == -2);
    fm_assert(math::floor((F)1.0000001) == 1);
    fm_assert(math::floor((F)-1.000001) == -2);
    fm_assert(math::floor((F)1e-8) == 0);
    fm_assert(math::floor((F)-1e-8) == -1);

    return true;
}

template<typename F>
constexpr bool test_ceil()
{
    fm_assert(math::ceil((F)-1.5) == -1);
    fm_assert(math::ceil((F)0) == 0);
    fm_assert(math::ceil((F)1) == 1);
    fm_assert(math::ceil((F)-1) == -1);
    fm_assert(math::ceil((F)-2) == -2);
    fm_assert(math::ceil((F)1.0000001) == 2);
    fm_assert(math::ceil((F)-1.000001) == -1);
    fm_assert(math::ceil((F)1e-8) == 1);
    fm_assert(math::ceil((F)-1e-8) == 0);

    return true;
}

} // namespace

void test_app::test_math()
{
    static_assert(test_double_sqrt());
    fm_assert(test_sqrt<float>());
    fm_assert(test_sqrt<double>());
    static_assert(test_floor<float>());
    static_assert(test_floor<double>());
    static_assert(test_ceil<float>());
    static_assert(test_ceil<double>());
}

} // namespace floormat
