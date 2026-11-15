#include "app.hpp"
#include "compat/pairwise.hpp"
#include <cr/Array.h>
#include <vector>

namespace floormat {

namespace {

struct nul_sentinel {};
struct c_string { const char* str; };

constexpr bool operator==(const char* p, nul_sentinel) noexcept { return *p == '\0'; }
constexpr const char* begin(const c_string& s) noexcept { return s.str; }
constexpr nul_sentinel end(const c_string&) noexcept { return {}; }

template<typename R>
constexpr u32 pair_count(R&& r)
{
    u32 n = 0;
    for ([[maybe_unused]] const auto& [a, b] : pairwise(r))
        n++;
    return n;
}

constexpr bool test_count()
{
    int xs[5] = {};
    return pair_count(xs) == 4 && pair_count(ArrayView<int>{xs, 2}) == 1 &&
           pair_count(ArrayView<int>{xs, 1}) == 0 && pair_count(ArrayView<int>{}) == 0;
}
static_assert(test_count());

constexpr int weighted_sum()
{
    const int xs[] = { 2, 3, 5, 7, 11 };
    int sum = 0, k = 1;
    for (const auto& [a, b] : pairwise(xs))
        sum += k++ * a * b;
    return sum;
}
static_assert(weighted_sum() == 1*2*3 + 2*3*5 + 3*5*7 + 4*7*11);

constexpr int sentinel_pairs()
{
    int code = 0;
    for (auto [a, b] : pairwise(c_string{"wxyz"}))
        code = code * 100 + (a - 'v') * 10 + (b - 'v');
    return code;
}
static_assert(sentinel_pairs() == 122334);

void test_mutation()
{
    int xs[] = { 13, 17, 19, 23 };
    for (auto& [a, b] : pairwise(xs))
        b += a;
    fm_assert(xs[0] == 13 && xs[1] == 30 && xs[2] == 49 && xs[3] == 72);
}

void test_containers()
{
    const Array<int> arr{InPlaceInit, { 29, 31, 37, 41, 43 }};
    int gaps = 0;
    for (const auto& [a, b] : pairwise(arr))
        gaps = gaps * 10 + (b - a);
    fm_assert(gaps == 2642);

    std::vector<int> v = { 47, 53, 59 };
    u32 n = 0;
    for (const auto& [a, b] : pairwise(v))
    {
        fm_assert(&a == &v[n] && &b == &v[n + 1]);
        n++;
    }
    fm_assert(n == 2);

    int last = 0;
    for (const auto& [a, b] : pairwise(Array<int>{InPlaceInit, { 61, 67 }}))
        last = a * b;
    fm_assert(last == 61 * 67);
}

} // namespace

void Test::test_pairwise()
{
    test_mutation();
    test_containers();
}

} // namespace floormat
