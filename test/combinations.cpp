#include "app.hpp"
#include "compat/array-size.hpp"
#include "compat/combinations.hpp"
#include <cr/Array.h>
#include <array>
#include <vector>

namespace floormat {

namespace {

struct adl_range { int xs[3]; };

constexpr int* begin(adl_range& r) noexcept { return r.xs; }
constexpr int* end(adl_range& r) noexcept { return r.xs + 3; }
constexpr const int* begin(const adl_range& r) noexcept { return r.xs; }
constexpr const int* end(const adl_range& r) noexcept { return r.xs + 3; }

struct nul_sentinel {};
struct c_string { const char* str; };

constexpr bool operator==(const char* p, nul_sentinel) noexcept { return *p == '\0'; }
constexpr const char* begin(const c_string& s) noexcept { return s.str; }
constexpr nul_sentinel end(const c_string&) noexcept { return {}; }

template<typename R>
constexpr u32 pair_count(R&& r)
{
    u32 n = 0;
    for ([[maybe_unused]] const auto& [a, b] : combinations(r))
        n++;
    return n;
}

constexpr bool test_count()
{
    int xs[7] = {};
    return pair_count(xs) == 7*6/2 && pair_count(ArrayView<int>{xs, 2}) == 1 &&
           pair_count(ArrayView<int>{xs, 1}) == 0 && pair_count(ArrayView<int>{}) == 0;
}
static_assert(test_count());

constexpr int weighted_sum()
{
    const int xs[] = { 2, 3, 5, 7 };
    int sum = 0, k = 1;
    for (const auto& [a, b] : combinations(xs))
        sum += k++ * a * b;
    return sum;
}
static_assert(weighted_sum() == 1*2*3 + 2*2*5 + 3*2*7 + 4*3*5 + 5*3*7 + 6*5*7);

void test_order()
{
    int xs[] = { 10, 11, 12, 13 };
    constexpr int expected[][2] = { {0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3} };
    u32 n = 0;
    for (auto& [a, b] : combinations(xs))
    {
        fm_assert(n < array_size(expected));
        fm_assert(&a == &xs[expected[n][0]] && &b == &xs[expected[n][1]]);
        n++;
    }
    fm_assert(n == array_size(expected));
}

void test_bindings()
{
    int xs[] = { 3, 5, 7 };

    for (auto& [a, b] : combinations(xs))
    {
        static_assert(std::is_same_v<decltype(a), int>);
        static_assert(std::is_same_v<decltype(b), int>);
        fm_assert(&a < &b);
    }

    for (auto&& [a, b] : combinations(xs))
    {
        static_assert(std::is_same_v<decltype(a), int>);
        a += 100;
        b += 1000;
    }
    fm_assert(xs[0] == 203 && xs[1] == 1105 && xs[2] == 2007);

    for (const auto& [a, b] : combinations(xs))
    {
        static_assert(std::is_same_v<decltype(a), const int>);
        static_assert(std::is_same_v<decltype(b), const int>);
        fm_assert(&a < &b);
    }

    for (auto [a, b] : combinations(xs))
    {
        static_assert(std::is_same_v<decltype(a), int>);
        fm_assert(&a != &xs[0] && &a != &xs[1] && &b != &xs[1] && &b != &xs[2]);
        a = 0;
        b = 0;
    }
    fm_assert(xs[0] == 203 && xs[1] == 1105 && xs[2] == 2007);
}

void test_const()
{
    const int cs[] = { 13, 17, 19 };
    for (auto& [a, b] : combinations(cs))
    {
        static_assert(std::is_same_v<decltype(a), const int>);
        fm_assert(&a < &b);
    }

    int ms[] = { 23, 29 };
    const auto c = combinations(ms);
    u32 n = 0;
    for (auto& [a, b] : c)
    {
        static_assert(std::is_same_v<decltype(a), const int>);
        fm_assert(&a == &ms[0] && &b == &ms[1]);
        n++;
    }
    fm_assert(n == 1);
}

void test_containers()
{
    Array<int> arr{InPlaceInit, { 31, 37, 41 }};
    int sum = 0;
    for (const auto& [a, b] : combinations(arr))
        sum += a * b;
    fm_assert(sum == 31*37 + 31*41 + 37*41);

    sum = 0;
    for (const auto& [a, b] : combinations(arrayView(arr).exceptPrefix(1)))
        sum += a * b;
    fm_assert(sum == 37*41);

    std::array<int, 3> sa = { 43, 47, 53 };
    sum = 0;
    for (const auto& [a, b] : combinations(sa))
        sum += a * b;
    fm_assert(sum == 43*47 + 43*53 + 47*53);

    std::vector<int> v = { 59, 61, 67 };
    sum = 0;
    for (const auto& [a, b] : combinations(v))
        sum += a * b;
    fm_assert(sum == 59*61 + 59*67 + 61*67);
}

void test_adl_sentinel()
{
    adl_range r = {{ 71, 73, 79 }};
    u32 n = 0;
    for (auto& [a, b] : combinations(r))
    {
        static_assert(std::is_same_v<decltype(a), int>);
        fm_assert(&a < &b);
        n++;
    }
    fm_assert(n == 3);

    constexpr const char* str = "pqrs";
    n = 0;
    for (const auto& [a, b] : combinations(c_string{str}))
    {
        static_assert(std::is_same_v<decltype(a), const char>);
        fm_assert(&a >= str && &a < &b && &b < str + 4);
        n++;
    }
    fm_assert(n == 6);
}

void test_rvalue_range()
{
    // Stored outside the loop, so a view that kept a pointer to the temporary Array would read freed memory.
    auto owned = combinations(Array<int>{InPlaceInit, { 83, 89, 97 }});
    int sum = 0;
    for (const auto& [a, b] : owned)
        sum += a * b;
    fm_assert(sum == 83*89 + 83*97 + 89*97);

    sum = 0;
    for (const auto& [a, b] : combinations(std::array<int, 2>{ 101, 103 }))
        sum += a * b;
    fm_assert(sum == 101*103);
}

} // namespace

void Test::test_combinations()
{
    test_order();
    test_bindings();
    test_const();
    test_containers();
    test_adl_sentinel();
    test_rvalue_range();
}

} // namespace floormat
