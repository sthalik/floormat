#include "app.hpp"
#include "compat/zip.hpp"
#include "compat/enumerate.hpp"
#include <cr/Array.h>

namespace floormat {

namespace {

struct count_iterator { uint32_t i; };
struct count_range { uint32_t n; };

constexpr uint32_t operator*(const count_iterator& it) noexcept { return it.i; }
constexpr count_iterator& operator++(count_iterator& it) noexcept { ++it.i; return it; }
constexpr bool operator==(const count_iterator& a, const count_iterator& b) noexcept { return a.i == b.i; }
constexpr count_iterator begin(const count_range&) noexcept { return {0}; }
constexpr count_iterator end(const count_range& r) noexcept { return {r.n}; }

struct nul_sentinel {};
struct c_string { const char* str; };

constexpr bool operator==(const char* p, nul_sentinel) noexcept { return *p == '\0'; }
constexpr const char* begin(const c_string& s) noexcept { return s.str; }
constexpr nul_sentinel end(const c_string&) noexcept { return {}; }

static_assert(detail_zip::sized_sentinel_for<const int*, const int*>);
static_assert(!detail_zip::sized_sentinel_for<count_iterator, count_iterator>);
static_assert(!detail_zip::sized_sentinel_for<nul_sentinel, const char*>);

constexpr int weighted_dot()
{
    int a[] = { 2, 3, 5 };
    const int b[] = { 7, 11, 13 };
    for (auto& [x, y] : zip(a, b))
        x *= y;
    int sum = 0;
    for (const auto& [i, x] : zip(count_range{3}, a))
        sum += (int)(i + 1) * x;
    return sum;
}
static_assert(weighted_dot() == 1*14 + 2*33 + 3*65);

void test_bindings()
{
    int a[] = { 3, 5, 7 };
    float b[] = { 11, 13, 17 };

    uint32_t n = 0;
    for (auto& [x, y] : zip(a, b))
    {
        static_assert(std::is_same_v<decltype(x), int>);
        static_assert(std::is_same_v<decltype(y), float>);
        fm_assert(&x == &a[n] && &y == &b[n]);
        n++;
    }
    fm_assert(n == 3);

    for (auto&& [x, y] : zip(a, b))
    {
        static_assert(std::is_same_v<decltype(x), int>);
        x += 100;
        y += 200;
    }
    fm_assert(a[0] == 103 && a[1] == 105 && a[2] == 107);
    fm_assert(b[0] == 211 && b[1] == 213 && b[2] == 217);

    n = 0;
    for (const auto& [x, y] : zip(a, b))
    {
        static_assert(std::is_same_v<decltype(x), const int>);
        static_assert(std::is_same_v<decltype(y), const float>);
        fm_assert(&x == &a[n] && &y == &b[n]);
        n++;
    }

    n = 0;
    for (auto [x, y] : zip(a, b))
    {
        static_assert(std::is_same_v<decltype(x), int>);
        fm_assert(&x != &a[n] && x == a[n] && y == b[n]);
        x = 0;
        y = 0;
        n++;
    }
    fm_assert(a[0] == 103 && a[1] == 105 && a[2] == 107);
    fm_assert(b[0] == 211 && b[1] == 213 && b[2] == 217);
}

void test_const()
{
    const int c[] = { 19, 23 };
    int m[] = { 29, 31 };

    uint32_t n = 0;
    for (auto& [x, y] : zip(c, m))
    {
        static_assert(std::is_same_v<decltype(x), const int>);
        static_assert(std::is_same_v<decltype(y), int>);
        fm_assert(&x == &c[n] && &y == &m[n]);
        n++;
    }
    fm_assert(n == 2);

    const auto cv = zip(m, m);
    for (auto& [x, y] : cv)
    {
        static_assert(std::is_same_v<decltype(x), const int>);
        static_assert(std::is_same_v<decltype(y), const int>);
        fm_assert(&x == &y);
    }
}

void test_rvalue_ranges()
{
    Array<int> arr{InPlaceInit, { 37, 41, 43, 47 }};

    // Stored outside the loop, see test_rvalue_range() in test/enumerate.cpp.
    auto z = zip(ArrayView<int>{arr}.exceptPrefix(1), ArrayView<int>{arr}.exceptSuffix(1));
    uint32_t n = 0;
    for (auto& [next, prev] : z)
    {
        fm_assert(&next == &arr[n + 1] && &prev == &arr[n]);
        n++;
    }
    fm_assert(n == 3);

    auto owned = zip(Array<int>{InPlaceInit, { 53, 59 }}, arr.prefix(2));
    int sum = 0;
    for (auto& [x, y] : owned)
        sum += x * y;
    fm_assert(sum == 53*37 + 59*41);
}

void test_prvalue()
{
    int xs[] = { 61, 67, 71 };

    uint32_t n = 0;
    for (auto& [i, x] : zip(count_range{3}, xs))
    {
        static_assert(std::is_same_v<decltype(i), uint32_t>);
        fm_assert(i == n++);
        fm_assert(&x == &xs[i]);
    }
    fm_assert(n == 3);

    for (const auto& [i, x] : zip(count_range{3}, xs))
    {
        static_assert(std::is_same_v<decltype(i), const uint32_t>);
        static_assert(std::is_same_v<decltype(x), const int>);
        fm_assert(&x == &xs[i]);
    }
}

void test_sentinel()
{
    constexpr const char* str = "pqr";
    uint32_t n = 0;
    for (auto& [c, i] : zip(c_string{str}, count_range{3}))
    {
        static_assert(std::is_same_v<decltype(c), const char>);
        fm_assert(&c == str + i);
        n++;
    }
    fm_assert(n == 3);
}

void test_arity()
{
    int a[] = { 73, 79 };
    short b[] = { 83, 89 };
    double c[] = { 97, 101 };
    const char d[] = { 'a', 'b' };
    uint32_t n = 0;
    for (auto& [w, x, y, z] : zip(a, b, c, d))
    {
        static_assert(std::is_same_v<decltype(x), short>);
        static_assert(std::is_same_v<decltype(y), double>);
        static_assert(std::is_same_v<decltype(z), const char>);
        fm_assert(&w == &a[n] && &x == &b[n] && &y == &c[n] && &z == &d[n]);
        n++;
    }
    fm_assert(n == 2);
}

void test_empty()
{
    uint32_t n = 0;
    for ([[maybe_unused]] auto& [x, y] : zip(ArrayView<int>{}, ArrayView<float>{}))
        n++;
    fm_assert(n == 0);
}

void test_enumerate_zip()
{
    int a[] = { 103, 107, 109 };
    float b[] = { 113, 127, 131 };

    uint32_t n = 0;
    for (auto& [i, e] : enumerate(zip(a, b)))
    {
        auto& [x, y] = e;
        static_assert(std::is_same_v<decltype(x), int>);
        static_assert(std::is_same_v<decltype(y), float>);
        fm_assert(i == n++);
        fm_assert(&x == &a[i] && &y == &b[i]);
        x += 1;
    }
    fm_assert(n == 3);
    fm_assert(a[0] == 104 && a[1] == 108 && a[2] == 110);

    for (const auto& [i, e] : enumerate(zip(a, b)))
    {
        auto& [x, y] = e;
        static_assert(std::is_same_v<decltype(x), const int>);
        static_assert(std::is_same_v<decltype(y), const float>);
        fm_assert(&x == &a[i] && &y == &b[i]);
    }
}

void test_zip_enumerate()
{
    int a[] = { 137, 139, 149 };
    float b[] = { 151, 157, 163 };

    uint32_t n = 0;
    for (auto& [e, y] : zip(enumerate(a), b))
    {
        auto& [i, x] = e;
        static_assert(std::is_same_v<decltype(i), uint32_t>);
        static_assert(std::is_same_v<decltype(x), int>);
        static_assert(std::is_same_v<decltype(y), float>);
        fm_assert(i == n++);
        fm_assert(&x == &a[i] && &y == &b[i]);
        x += 1;
    }
    fm_assert(n == 3);
    fm_assert(a[0] == 138 && a[1] == 140 && a[2] == 150);

    for (const auto& [e, y] : zip(enumerate(a), b))
    {
        auto& [i, x] = e;
        static_assert(std::is_same_v<decltype(i), const uint32_t>);
        static_assert(std::is_same_v<decltype(x), const int>);
        static_assert(std::is_same_v<decltype(y), const float>);
        fm_assert(&x == &a[i] && &y == &b[i]);
    }

    for (auto [e, y] : zip(enumerate(a), b))
    {
        auto& [i, x] = e;
        fm_assert(&x == &a[i] && &y != &b[i]);
        y = 0;
    }
    fm_assert(b[0] == 151 && b[1] == 157 && b[2] == 163);
}

} // namespace

void Test::test_zip()
{
    test_bindings();
    test_const();
    test_rvalue_ranges();
    test_prvalue();
    test_sentinel();
    test_arity();
    test_empty();
    test_enumerate_zip();
    test_zip_enumerate();
}

} // namespace floormat
