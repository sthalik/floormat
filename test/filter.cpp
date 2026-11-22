#include "app.hpp"
#include "compat/filter.hpp"
#include "compat/enumerate.hpp"
#include <cr/Array.h>
#include <cr/Pointer.h>
#include <functional>
#include <vector>

namespace floormat {

namespace {

struct nul_sentinel {};
struct c_string { const char* str; };

constexpr bool operator==(const char* p, nul_sentinel) noexcept { return *p == '\0'; }
constexpr const char* begin(const c_string& s) noexcept { return s.str; }
constexpr nul_sentinel end(const c_string&) noexcept { return {}; }

struct greater_than
{
    int limit;
    constexpr bool operator()(const int& x) const noexcept { return x > limit; }
};

constexpr bool is_odd(const int& x) { return x % 2 != 0; }

template<typename V>
constexpr inline bool has_const_begin = requires(const V& v) { v.begin(); };

template<typename R, typename P>
constexpr u64 concat_kept_elements(R&& r, const P& pred)
{
    u64 code = 0;
    for (int x : filter(pred, forward<R>(r)))
        code = code * 100 + u64(x);
    return code;
}

constexpr bool test_kept_elements()
{
    const int xs[] = { 13, 17, 19, 23, 29 };
    int num_pred_calls = 0;
    auto counting_predicate = [&num_pred_calls](const int& x) { num_pred_calls++; return x % 3 != 1; };
    return concat_kept_elements(xs, counting_predicate) == 172329 && num_pred_calls == 5 &&
           concat_kept_elements(xs, [](const int&) { return true; }) == 1317192329 &&
           concat_kept_elements(xs, [](const int&) { return false; }) == 0 &&
           concat_kept_elements(xs, [](const int& x) { return x == 13; }) == 13 &&
           concat_kept_elements(xs, [](const int& x) { return x == 29; }) == 29 &&
           concat_kept_elements(xs, is_odd) == 1317192329 &&
           concat_kept_elements(ArrayView<const int>{}, counting_predicate) == 0 && num_pred_calls == 5;
}
static_assert(test_kept_elements());

constexpr bool test_function_predicates()
{
    const int xs[] = { 2, 3, 5, 8 };
    u64 code = 0;
    for (int x : filter(is_odd, xs))
        code = code * 100 + u64(x);
    for (int x : filter(&is_odd, xs))
        code = code * 100 + u64(x);
    return code == 3050305;
}
static_assert(test_function_predicates());

struct item
{
    int value;
    bool is_kept;
    constexpr bool is_value_odd() const noexcept { return value % 2 != 0; }
};

constexpr bool test_member_predicates()
{
    const item items[] = { { 3, true }, { 4, false }, { 5, false }, { 6, true } };
    const item* const pointers[] = { &items[0], &items[1], &items[2], &items[3] };
    const std::reference_wrapper<const item> refs[] = { items[0], items[1], items[2], items[3] };
    int code = 0;
    for (const item& x : filter(&item::is_value_odd, items))
        code = code * 10 + x.value;
    for (const item& x : filter(&item::is_kept, items))
        code = code * 10 + x.value;
    for (const item* p : filter(&item::is_value_odd, pointers))
        code = code * 10 + p->value;
    for (const item& x : filter(&item::is_kept, refs))
        code = code * 10 + x.value;
    return code == 35363536;
}
static_assert(test_member_predicates());

constexpr int sentinel_chars()
{
    int code = 0;
    for (char c : filter([](const char& ch) { return ch != 'a'; }, c_string{"banana"}))
        code = code * 100 + c;
    return code;
}
static_assert(sentinel_chars() == 'b'*10000 + 'n'*100 + 'n');

constexpr bool test_const_view()
{
    const int xs[] = { 31, 37, 41, 43 };
    auto pred = [limit = 40](const int& x) { return x > limit; };
    static_assert(std::is_copy_assignable_v<decltype(filter(pred, xs))>);
    const auto v = filter(pred, xs);
    int sum = 0;
    for (int x : v)
        sum += x;
    for (int x : v)
        sum -= 2*x;
    return sum == -(41 + 43);
}
static_assert(test_const_view());

constexpr bool test_lvalue_predicate_by_reference()
{
    const int xs[] = { 11, 22, 33, 44 };
    greater_than pred{30};
    auto v = filter(pred, xs);
    pred.limit = 20;
    int sum = 0;
    for (int x : v)
        sum += x;
    return sum == 22 + 33 + 44;
}
static_assert(test_lvalue_predicate_by_reference());

constexpr auto filter_above_75(const int (&xs)[4])
{
    return filter(greater_than{75}, xs);
}

constexpr bool test_returned_view()
{
    const int xs[] = { 71, 73, 79, 83 };
    auto v = filter_above_75(xs);
    int sum = 0;
    for (int x : v)
        sum += x;
    for (int x : v)
        sum += x;
    return sum == 2*(79 + 83);
}
static_assert(test_returned_view());

void test_mutation()
{
    int xs[] = { 13, 17, 19, 23 };
    auto pred = [](int& y) { return y > 15 && y < 20; };
    static_assert(!has_const_begin<decltype(filter(pred, Array<int>{}))>);
    static_assert(has_const_begin<decltype(filter(greater_than{0}, Array<int>{}))>);
    for (int& x : filter(pred, xs))
        x += 100;
    fm_assert(xs[0] == 13 && xs[1] == 117 && xs[2] == 119 && xs[3] == 23);
}

void test_containers()
{
    auto pred = [](const int& x) { return x % 4 == 1; };

    const Array<int> arr{InPlaceInit, { 29, 31, 37, 41, 43 }};
    fm_assert(concat_kept_elements(arr, pred) == 293741);

    std::vector<int> vec = { 47, 53, 59, 61 };
    u32 n = 0;
    for (int& x : filter(pred, vec))
    {
        fm_assert(&x == &vec[2*n + 1]);
        n++;
    }
    fm_assert(n == 2);

    fm_assert(concat_kept_elements(Array<int>{InPlaceInit, { 67, 73, 79 }}, pred) == 73);
}

void test_over_enumerate()
{
    int xs[] = { 31, 37, 41, 43, 47 };
    auto pred = [](const auto& e) { const auto& [j, y] = e; return j % 2 == 0; };
    static_assert(has_const_begin<decltype(filter(pred, enumerate(xs)))>);

    u32 code = 0;
    for (auto& [i, x] : filter(pred, enumerate(xs)))
    {
        code = code * 1000 + i * 100 + u32(x);
        x = -x;
    }
    fm_assert(code == 31241447);
    fm_assert(xs[0] == -31 && xs[1] == 37 && xs[2] == -41 && xs[3] == 43 && xs[4] == -47);
}

template<typename R>
void test_temporary_predicate(R&& xs)
{
    int sum = 0;
    for (int x : filter([limit = 60](const int& y) { return y > limit; }, xs))
        sum += x;
    fm_assert(sum == 61 + 67);
}

void test_move_only_predicate()
{
    const int xs[] = { 41, 43, 47, 53 };
    auto v = filter([limit = Pointer<int>{InPlaceInit, 45}](const int& x) { return x > *limit; }, xs);
    static_assert(!std::is_copy_constructible_v<decltype(v)>);
    auto w = move(v);
    int sum = 0;
    for (int x : w)
        sum += x;
    fm_assert(sum == 47 + 53);
}

} // namespace

void Test::test_filter()
{
    test_mutation();
    test_containers();
    test_over_enumerate();
    const int xs[] = { 53, 59, 61, 67 };
    test_temporary_predicate(xs);
    test_temporary_predicate(std::vector<int>{ 53, 59, 61, 67 });
    test_move_only_predicate();
}

} // namespace floormat
