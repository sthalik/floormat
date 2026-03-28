#include "app.hpp"
#include "compat/float.hpp"
#include <mg/Functions.h>
#include <concepts>
#include <bit>
#include <array>
#include <limits>

namespace floormat::Test {
namespace {

bool test_fpclassify()
{
    fm_assert_equal(FP_ZERO,     fpclassify(0.f));
    fm_assert_equal(FP_NORMAL,   fpclassify(1.f));
    fm_assert_equal(FP_NORMAL,   fpclassify(FLT_MIN));
#ifndef __FAST_MATH__
    fm_assert_equal(FP_INFINITE, fpclassify(std::numeric_limits<float>::infinity()));
#endif

    return true;
}

float slow_nth_float(float x, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++)
        x = std::nextafter(x, float{1<<24});
    return x;
}

void test_float_zero()
{
    constexpr auto steps = std::array{0u, 1u, 2u, 3u, 8u, 33u, 513u};

    for (uint32_t n : steps)
    {
        const volatile float got = nth_float(FLT_MIN, n);
        const volatile float expected = slow_nth_float(FLT_MIN, n);
        fm_assert_equal(got, expected);
        //fm_assert(std::bit_cast<uint32_t>(got) == n);
    }
}

void test_float_normal()
{
    const volatile float x = 0.5f;
    constexpr auto steps = std::array{0u, 1u, 2u, 3u, 9u, 17u, 129u, 1025u, 4096u};

    const uint32_t ux = std::bit_cast<uint32_t>((float)x);
    float prev = x;

    for (uint32_t n : steps)
    {
        const float got = nth_float(x, n);
        const float expected = slow_nth_float(x, n);
        const uint32_t ugot = std::bit_cast<uint32_t>(got);

        fm_assert_equal(got, expected);
        fm_assert(ugot == ux + n);
        fm_assert(n == 0 ? got == x : got > x);
        fm_assert(got < 1.f);
        fm_assert(n == 0 ? got == prev : got > prev);
        prev = got;
    }
}

void test_float_upper_bound()
{
    const volatile float one_prev = std::nextafter(1.f, 0.f);
    const volatile float two_prev = std::nextafter(one_prev, 0.f);
    const uint32_t u = std::bit_cast<uint32_t>((float)two_prev);

    fm_assert(one_prev < 1.f);
    fm_assert(two_prev < one_prev);

    fm_assert_equal(one_prev, nth_float(one_prev, 0));
    fm_assert_equal(1.f,      nth_float(one_prev, 1));
    fm_assert_equal(one_prev, nth_float(two_prev, 1));
    fm_assert_equal(1.f,      nth_float(two_prev, 2));
    fm_assert_equal(slow_nth_float(two_prev, 2), nth_float(two_prev, 2));

    fm_assert(std::bit_cast<uint32_t>(nth_float(two_prev, 1)) == u + 1u);
    fm_assert(std::bit_cast<uint32_t>(nth_float(two_prev, 2)) == u + 2u);
}
} // namespace


void test_float()
{
    test_fpclassify();
    test_float_zero();
    test_float_normal();
    test_float_upper_bound();
}

} // namespace floormat::Test
