#include "app.hpp"
#include "compat/all-of.hpp"
#include "compat/any-of.hpp"
#include "compat/contains.hpp"
#include "compat/count-if.hpp"
#include "compat/equal.hpp"
#include "compat/find-if.hpp"
#include <cr/Array.h>
#include <cr/StridedArrayView.h>
#include <cr/StringView.h>
#include <array>
#include <vector>

namespace floormat {

// In floormat itself, so that its hidden friends share a namespace with floormat::begin and floormat::end.
struct range_algorithms_friend_range
{
    int xs[3];
    friend constexpr const int* begin(const range_algorithms_friend_range& r) noexcept { return r.xs; }
    friend constexpr const int* end(const range_algorithms_friend_range& r) noexcept { return r.xs + 3; }
};

namespace {

struct adl_range { int xs[3]; };

constexpr const int* begin(const adl_range& r) noexcept { return r.xs; }
constexpr const int* end(const adl_range& r) noexcept { return r.xs + 3; }

struct nul_sentinel {};
struct c_string { const char* str; };

constexpr bool operator==(const char* p, nul_sentinel) noexcept { return *p == '\0'; }
constexpr const char* begin(const c_string& s) noexcept { return s.str; }
constexpr nul_sentinel end(const c_string&) noexcept { return {}; }

struct node { int value; const node* next; };
struct list_iterator { const node* n; };
struct list { const node* head; };

constexpr const int& operator*(const list_iterator& it) noexcept { return it.n->value; }
constexpr list_iterator& operator++(list_iterator& it) noexcept { it.n = it.n->next; return it; }
constexpr bool operator==(const list_iterator& a, const list_iterator& b) noexcept { return a.n == b.n; }
constexpr list_iterator begin(const list& l) noexcept { return {l.head}; }
constexpr list_iterator end(const list&) noexcept { return {nullptr}; }

template<typename R> concept begin_callable = requires(R&& r) { floormat::begin(forward<R>(r)); };

static_assert(range<int[3]>);
static_assert(range<const int[3]>);
static_assert(range<Array<int>>);
static_assert(range<std::vector<int>>);
static_assert(range<const adl_range>);
static_assert(range<const c_string>);
static_assert(range<const list>);
static_assert(range<const range_algorithms_friend_range>);
static_assert(!range<int>);
static_assert(!range<int*>);
static_assert(begin_callable<Array<int>&>);
static_assert(!begin_callable<Array<int>>);

static_assert(std::is_same_v<iterator_t<int[3]>, int*>);
static_assert(std::is_same_v<iterator_t<const Array<int>>, const int*>);
static_assert(std::is_same_v<sentinel_t<const c_string>, nul_sentinel>);

static_assert(sized_range<int[3]>);
static_assert(sized_range<ArrayView<const int>>);
static_assert(sized_range<std::array<int, 3>>);
static_assert(!sized_range<const adl_range>);
static_assert(!sized_range<const c_string>);

static_assert(sized_sentinel_for<const int*, const int*>);
static_assert(!sized_sentinel_for<nul_sentinel, const char*>);
static_assert(!sized_sentinel_for<list_iterator, list_iterator>);

static_assert(random_access_iterator<int*>);
static_assert(random_access_iterator<std::vector<int>::const_iterator>);
static_assert(random_access_iterator<std::array<int, 3>::iterator>);
static_assert(!random_access_iterator<list_iterator>);
static_assert(random_access_range<ArrayView<const int>>);
static_assert(random_access_range<const c_string>);
static_assert(!random_access_range<const list>);

constexpr node n3{19, nullptr}, n2{17, &n3}, n1{13, &n2};
constexpr list primes{&n1};

constexpr bool test_contains()
{
    constexpr int xs[] = { 3, 5, 7 };
    return contains(xs, 3) && contains(xs, 7) && !contains(xs, 4) &&
           contains(c_string{"pqr"}, 'r') && !contains(c_string{"pqr"}, '\0') &&
           contains(primes, 19) && !contains(primes, 23) &&
           contains(range_algorithms_friend_range{{29, 31, 37}}, 31) &&
           !contains(ArrayView<const int>{}, 0);
}
static_assert(test_contains());

constexpr bool test_any_all()
{
    constexpr int xs[] = { 41, 43, 47 };
    constexpr auto odd = [](int x) { return x % 2 != 0; };
    constexpr auto above_42 = [](int x) { return x > 42; };
    return any_of(xs, above_42) && !all_of(xs, above_42) && all_of(xs, odd) && !any_of(xs, [](int x) { return x == 0; }) &&
           !any_of(ArrayView<const int>{}, odd) && all_of(ArrayView<const int>{}, [](int) { return false; }) &&
           any_of(primes, [](int x) { return x == 17; }) && all_of(adl_range{{53, 59, 61}}, odd);
}
static_assert(test_any_all());

constexpr bool test_count_if()
{
    constexpr int xs[] = { 67, 71, 73, 79, 83 };
    return count_if(xs, [](int x) { return x % 10 == 3; }) == 2 && count_if(xs, [](int) { return true; }) == 5 &&
           count_if(ArrayView<const int>{}, [](int) { return true; }) == 0 &&
           count_if(c_string{"a-b-c"}, [](char c) { return c == '-'; }) == 2 &&
           count_if(primes, [](int x) { return x > 13; }) == 2;
}
static_assert(test_count_if());

constexpr bool test_equal()
{
    constexpr int xs[] = { 89, 97, 101 };
    constexpr int ys[] = { 89, 97, 101 };
    constexpr int zs[] = { 89, 97, 103 };
    constexpr int prefix[] = { 89, 97 };
    return equal(xs, ys) && !equal(xs, zs) && !equal(xs, prefix) && !equal(prefix, xs) &&
           equal(ArrayView<const int>{}, ArrayView<const int>{}) && equal(ArrayView<const int>{xs}.prefix(2), prefix);
}
static_assert(test_equal());

struct prime { int value; bool is_twin; };

constexpr bool test_find_if()
{
    constexpr int xs[] = { 151, 157, 163, 167 };
    constexpr prime ps[] = { {173, false}, {179, true}, {181, true} };
    constexpr auto above_160 = [](int x) { return x > 160; };
    constexpr auto is_v = [](char c) { return c == 'v'; };
    const char* s = "uvw";
    const auto first = floormat::begin(primes), last = floormat::end(primes);
    return find_if(xs, xs + 4, above_160) == xs + 2 && find_if(xs + 3, xs + 4, above_160) == xs + 3 &&
           find_if(xs, xs + 4, [](int x) { return x > 167; }) == xs + 4 && find_if(xs, xs, above_160) == xs &&
           find_if(s, nul_sentinel{}, is_v) == s + 1 && *find_if(s + 2, nul_sentinel{}, is_v) == '\0' &&
           *find_if(first, last, [](int x) { return x > 13; }) == 17 &&
           find_if(first, last, [](int x) { return x > 19; }) == last &&
           find_if(ps, ps + 3, &prime::is_twin) == ps + 1;
}
static_assert(test_find_if());

struct item
{
    int value;
    bool is_kept;
    constexpr bool is_value_odd() const noexcept { return value % 2 != 0; }
};

constexpr bool test_member_predicates()
{
    const item items[] = { { 13, true }, { 14, false }, { 15, false }, { 16, true }, { 17, false } };
    const item* const pointers[] = { &items[0], &items[1], &items[2], &items[3], &items[4] };
    return any_of(items, &item::is_kept) && !all_of(items, &item::is_kept) &&
           any_of(items, &item::is_value_odd) && !all_of(items, &item::is_value_odd) &&
           count_if(items, &item::is_kept) == 2 && count_if(items, &item::is_value_odd) == 3 &&
           any_of(pointers, &item::is_kept) && !all_of(pointers, &item::is_value_odd) &&
           count_if(pointers, &item::is_kept) == 2 && count_if(pointers, &item::is_value_odd) == 3 &&
           !any_of(ArrayView<const item>{}, &item::is_kept) && all_of(ArrayView<const item>{}, &item::is_kept);
}
static_assert(test_member_predicates());

void test_stops_early()
{
    const int xs[] = { 107, 109, 113, 127 };
    u32 calls = 0;
    fm_assert(any_of(xs, [&](int x) { calls++; return x == 109; }));
    fm_assert(calls == 2);
    calls = 0;
    fm_assert(!all_of(xs, [&](int x) { calls++; return x < 113; }));
    fm_assert(calls == 3);
    calls = 0;
    fm_assert(count_if(xs, [&](int x) { calls++; return x > 110; }) == 2);
    fm_assert(calls == 4);
    calls = 0;
    fm_assert(find_if(xs, xs + 4, [&](int x) { calls++; return x > 110; }) == xs + 2);
    fm_assert(calls == 3);
}

void test_containers()
{
    Array<int> arr{InPlaceInit, { 131, 137, 139 }};
    std::vector<int> v = { 131, 137, 139 };
    std::array<int, 3> sa = { 131, 137, 149 };
    fm_assert(contains(arr, 137) && contains(v, 139) && !contains(sa, 139));
    fm_assert(equal(arr, v) && equal(v, arr) && !equal(arr, sa));
    fm_assert(equal(arrayView(arr).exceptSuffix(1), ArrayView<const int>{sa.data(), 2}));
    fm_assert(count_if(stridedArrayView(arr).every(2), [](int x) { return x > 135; }) == 1);
    fm_assert(all_of(stridedArrayView(arr).flipped<0>(), [](int x) { return x > 130; }));
    fm_assert(floormat::find_if(v.begin(), v.end(), [](int x) { return x > 135; }) == v.begin() + 1);
    auto sv = stridedArrayView(arr).every(2);
    fm_assert(*floormat::find_if(sv.begin(), sv.end(), [](int x) { return x > 135; }) == 139);

    const char* args[] = { "--foo", "--magnum-log=quiet" };
    fm_assert(contains(args, "--magnum-log=quiet"_s) && !contains(args, "--bar"_s));
    fm_assert(any_of(args, [](StringView s) { return s.hasPrefix("--magnum-"_s); }));
}

} // namespace

void Test::test_range_algorithms()
{
    test_stops_early();
    test_containers();
}

} // namespace floormat
