#include "app.hpp"
#include "compat/enumerate.hpp"
#include <cr/Array.h>

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

constexpr uint32_t weighted_sum()
{
    int xs[] = { 2, 3, 5, 7 };
    for (auto& [i, x] : enumerate(xs))
        x *= (int)i + 11;
    uint32_t sum = 0;
    for (const auto& [i, x] : enumerate(xs))
        sum += (i + 1) * (uint32_t)x;
    return sum;
}
static_assert(weighted_sum() == 1*22 + 2*36 + 3*65 + 4*98);

void test_bindings()
{
    int xs[] = { 3, 5, 7, 11 };

    uint32_t n = 0;
    for (auto& [i, x] : enumerate(xs))
    {
        static_assert(std::is_same_v<decltype(i), uint32_t>);
        static_assert(std::is_same_v<decltype(x), int>);
        fm_assert(i == n++);
        fm_assert(&x == &xs[i]);
    }
    fm_assert(n == 4);

    for (auto&& [i, x] : enumerate(xs))
    {
        static_assert(std::is_same_v<decltype(x), int>);
        fm_assert(&x == &xs[i]);
        x += 100;
    }
    fm_assert(xs[0] == 103 && xs[1] == 105 && xs[2] == 107 && xs[3] == 111);

    for (const auto& [i, x] : enumerate(xs))
    {
        static_assert(std::is_same_v<decltype(i), const uint32_t>);
        static_assert(std::is_same_v<decltype(x), const int>);
        fm_assert(&x == &xs[i]);
    }

    for (auto [i, x] : enumerate(xs))
    {
        static_assert(std::is_same_v<decltype(x), int>);
        fm_assert(&x != &xs[i]);
        fm_assert(x == xs[i]);
        x = 0;
    }
    fm_assert(xs[0] == 103 && xs[1] == 105 && xs[2] == 107 && xs[3] == 111);
}

void test_const()
{
    const int cs[] = { 13, 17, 19 };
    for (auto& [i, x] : enumerate(cs))
    {
        static_assert(std::is_same_v<decltype(x), const int>);
        fm_assert(&x == &cs[i]);
    }

    int xs[] = { 23, 29, 31 };
    const auto cv = enumerate(xs);
    for (auto& [i, x] : cv)
    {
        static_assert(std::is_same_v<decltype(x), const int>);
        fm_assert(&x == &xs[i]);
    }
    auto mv = enumerate(xs);
    for (auto& [i, x] : mv)
    {
        static_assert(std::is_same_v<decltype(x), int>);
        fm_assert(&x == &xs[i]);
    }
}

void test_rvalue_range()
{
    Array<int> arr{InPlaceInit, { 37, 41, 43, 47 }};

    // Stored outside the loop, so a view that kept a pointer to the temporary ArrayView would read a dead stack slot.
    auto e = enumerate(ArrayView<int>{arr}.exceptPrefix(1));
    uint32_t n = 0;
    for (auto& [i, x] : e)
    {
        static_assert(std::is_same_v<decltype(x), int>);
        fm_assert(&x == &arr[i + 1]);
        n++;
    }
    fm_assert(n == 3);

    auto owned = enumerate(Array<int>{InPlaceInit, { 53, 59 }});
    uint32_t sum = 0;
    for (auto& [i, x] : owned)
        sum += (i + 1) * (uint32_t)x;
    fm_assert(sum == 53 + 2*59);
}

void test_empty()
{
    uint32_t n = 0;
    for ([[maybe_unused]] auto& [i, x] : enumerate(ArrayView<int>{}))
        n++;
    fm_assert(n == 0);
}

void test_adl()
{
    adl_range r{{ 61, 67, 71 }};
    for (auto& [i, x] : enumerate(r))
    {
        static_assert(std::is_same_v<decltype(x), int>);
        fm_assert(&x == &r.xs[i]);
    }
    const auto cv = enumerate(r);
    for (auto& [i, x] : cv)
    {
        static_assert(std::is_same_v<decltype(x), const int>);
        fm_assert(&x == &r.xs[i]);
    }

    constexpr const char* str = "xyz";
    uint32_t n = 0;
    for (auto& [i, c] : enumerate(c_string{str}))
    {
        static_assert(std::is_same_v<decltype(c), const char>);
        fm_assert(&c == str + i);
        n++;
    }
    fm_assert(n == 3);
}

} // namespace

void Test::test_enumerate()
{
    test_bindings();
    test_const();
    test_rvalue_range();
    test_empty();
    test_adl();
}

} // namespace floormat
