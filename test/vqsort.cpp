#include "app.hpp"
#include "src/hwy.hpp"
#include <algorithm>
#include <limits>
#include <random>
#include <type_traits>
#include <cr/Array.h>
#include <hwy/targets.h>

namespace floormat::Test {
namespace {

template<typename T> bool key_less(T a, T b) { return a < b; }
bool key_less(uint128_t a, uint128_t b) { return a.hi != b.hi ? a.hi < b.hi : a.lo < b.lo; }
bool key_less(K64V64 a, K64V64 b) { return a.key < b.key; }
bool key_less(K32V32 a, K32V32 b) { return a.key < b.key; }

template<typename T> bool full_less(T a, T b) { return key_less(a, b); }
bool full_less(K64V64 a, K64V64 b) { return a.key != b.key ? a.key < b.key : a.value < b.value; }
bool full_less(K32V32 a, K32V32 b) { return a.key != b.key ? a.key < b.key : a.value < b.value; }

template<typename T> bool same(T a, T b) { return a == b; }
bool same(uint128_t a, uint128_t b) { return a.lo == b.lo && a.hi == b.hi; }
bool same(K64V64 a, K64V64 b) { return a.value == b.value && a.key == b.key; }
bool same(K32V32 a, K32V32 b) { return a.value == b.value && a.key == b.key; }

template<typename T> T make_key(std::mt19937& rng, uint32_t)
{
    const auto r = rng() % 61;
    if (r == 0)
        return std::numeric_limits<T>::max();
    if (r == 1)
        return std::numeric_limits<T>::lowest();
    const auto x = int32_t(rng() % 401) - 200;
    if constexpr (std::is_floating_point_v<T>)
        return T(x) * T(0.25);
    else if constexpr (std::is_signed_v<T>)
        return T(x);
    else
        return T(x + 200);
}

template<> uint128_t make_key(std::mt19937& rng, uint32_t)
{
    const auto lo = (uint64_t)rng() << 32 | rng() % 7;
    const auto hi = rng() % 5 | (uint64_t)(rng() & 1) << 63;
    return { .lo = lo, .hi = hi };
}

template<> K64V64 make_key(std::mt19937& rng, uint32_t i)
{
    const auto key = rng() % 50 | (uint64_t)(rng() & 1) << 63;
    return { .value = i, .key = key };
}

template<> K32V32 make_key(std::mt19937& rng, uint32_t i)
{
    const auto key = (uint32_t)(rng() % 50 | (rng() & 1) << 31);
    return { .value = i, .key = key };
}

template<typename T>
void check(uint32_t n)
{
    std::mt19937 rng{n * 2654435761u + (uint32_t)sizeof(T)};
    Array<T> input{NoInit, n};
    for (auto i = 0u; i < n; i++)
        input[i] = make_key<T>(rng, i);

    Array<T> canonical{NoInit, n}, expected{NoInit, n}, a{NoInit, n}, b{NoInit, n};
    std::copy_n(input.data(), n, canonical.data());
    std::sort(canonical.data(), canonical.data() + n, [](T x, T y) { return full_less(x, y); });
    const auto check_permutation = [&] {
        std::copy_n(a.data(), n, b.data());
        std::sort(b.data(), b.data() + n, [](T x, T y) { return full_less(x, y); });
        for (auto i = 0u; i < n; i++)
            fm_assert(same(b[i], canonical[i]));
    };
    const auto equiv = [](T x, T y) { return !key_less(x, y) && !key_less(y, x); };

    for (const auto order : { sort_order::ascending, sort_order::descending })
    {
        const auto before = [order](T x, T y) { return order == sort_order::ascending ? key_less(x, y) : key_less(y, x); };
        std::copy_n(input.data(), n, expected.data());
        std::stable_sort(expected.data(), expected.data() + n, before);

        std::copy_n(input.data(), n, a.data());
        vqsort(a.data(), n, order);
        for (auto i = 0u; i < n; i++)
            fm_assert(equiv(a[i], expected[i]));
        check_permutation();

        if (n == 0)
            continue;

        const auto k = n / 3 + 1;
        std::copy_n(input.data(), n, a.data());
        vqsort_partial(a.data(), n, k, order);
        for (auto i = 0u; i < k; i++)
            fm_assert(equiv(a[i], expected[i]));
        check_permutation();

        const auto m = n / 2;
        std::copy_n(input.data(), n, a.data());
        vqselect(a.data(), n, m, order);
        fm_assert(equiv(a[m], expected[m]));
        for (auto i = 0u; i < m; i++)
            fm_assert(!before(a[m], a[i]));
        for (auto i = m + 1; i < n; i++)
            fm_assert(!before(a[i], a[m]));
        check_permutation();
    }
}

} // namespace

void test_vqsort()
{
    // 0 = real dispatch; EMU128/SCALAR force the std::sort fallback path.
    for (const int64_t forced : { 0LL, HWY_EMU128, HWY_SCALAR })
    {
        hwy::SetSupportedTargetsForTest(forced);
        for (const uint32_t n : { 0u, 1u, 2u, 33u, 1000u })
        {
            check<uint16_t>(n);
            check<int16_t>(n);
            check<uint32_t>(n);
            check<int32_t>(n);
            check<uint64_t>(n);
            check<int64_t>(n);
            check<float>(n);
            check<double>(n);
            check<uint128_t>(n);
            check<K64V64>(n);
            check<K32V32>(n);
        }
    }
    hwy::SetSupportedTargetsForTest(0);
}

} // namespace floormat::Test
