#include "app.hpp"
#include "compat/borrowed-ptr.inl"
#include "compat/container-traits.inl"
#include "compat/equal.hpp"
#include "compat/erase-if.hpp"
#include "compat/inplace-array.hpp"
#include <cr/GrowableArray.h>
#include <cr/String.h>
#include <initializer_list>

namespace floormat {

namespace {

struct triple
{
    u64 a, b, c;
    constexpr triple(u32 i) : a{i}, b{i * 3ull}, c{i * 5ull} {}
    bool operator==(const triple&) const = default;
};

int counted_alive = 0; // NOLINT

struct counted : bptr_base
{
    int x;
    explicit counted(int x) : x{x} { ++counted_alive; }
    ~counted() noexcept override { --counted_alive; }
};

template<typename Pred>
constexpr bool erases_to(std::initializer_list<int> in, Pred pred, std::initializer_list<int> expected)
{
    inplace_array<int, 16> v;
    for (int x : in)
        v.push_back(x);
    const usize removed = erase_if(v, pred);
    return removed == in.size() - expected.size() && equal(v, expected);
}

constexpr bool test_cases()
{
    constexpr auto even = [](int x) { return x % 2 == 0; };
    constexpr auto zero = [](int x) { return x == 0; };
    return erases_to({}, even, {}) &&
           erases_to({1, 3, 5}, even, {1, 3, 5}) &&
           erases_to({2, 4, 6}, even, {}) &&
           erases_to({1, 2, 3, 4, 5, 6, 7}, even, {1, 3, 5, 7}) &&
           erases_to({2, 4, 1, 3}, even, {1, 3}) &&
           erases_to({1, 3, 2, 4}, even, {1, 3}) &&
           erases_to({1, 2, 4, 6, 8, 3}, even, {1, 3}) &&
           erases_to({0, 1, 2, 3, 0, 4, 5, 6, 7, 0, 0, 8, 9, 10, 11, 12}, zero,
                     {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
}

static_assert(test_cases());

struct item
{
    int value;
    bool is_removed;
    constexpr bool is_value_odd() const noexcept { return value % 2 != 0; }
};

constexpr bool test_member_pointers()
{
    inplace_array<item, 16> a;
    for (int i = 0; i < 7; i++)
        a.push_back(item{13 + i, i % 3 == 0});
    if (erase_if(a, &item::is_removed) != 3 || a.size() != 4)
        return false;
    if (a[0].value != 14 || a[1].value != 15 || a[2].value != 17 || a[3].value != 18)
        return false;
    if (erase_if(a, &item::is_value_odd) != 2 || a.size() != 2)
        return false;
    return a[0].value == 14 && a[1].value == 18;
}
static_assert(test_member_pointers());

constexpr bool removes(u64 x) { return x % 11 == 0 || x % 7 == 3; }
constexpr u64 key(u32 x) { return x; }
constexpr u64 key(const triple& t) { return t.a; }

template<typename T>
void test_array()
{
    constexpr u32 size = 1000;
    Array<T> a, expected;
    arrayReserve(a, size);
    arrayReserve(expected, size);
    for (u32 i = 0; i < size; i++)
    {
        arrayAppend(a, T{i});
        if (!removes(i))
            arrayAppend(expected, T{i});
    }
    fm_assert(erase_if(a, [](const T& x) { return removes(key(x)); }) == size - expected.size());
    fm_assert(equal(a, expected));
}

void test_non_growable()
{
    Array<u32> a{InPlaceInit, { 1, 2, 3, 4 }};
    fm_assert(erase_if(a, [](u32 x) { return x > 9; }) == 0);
    fm_assert(!arrayIsGrowable(a));
    const u32 odd[] = { 1, 3 };
    fm_assert(erase_if(a, [](u32 x) { return x % 2 == 0; }) == 2);
    fm_assert(equal(a, odd));
}

void test_strings()
{
    constexpr auto long_string = [](u32 i) {
        const char digit = char('0' + i);
        return "a string too long for the small string buffer "_s + StringView{&digit, 1};
    };
    Array<String> a;
    arrayReserve(a, 10);
    for (u32 i = 0; i < 10; i++)
        arrayAppend(a, long_string(i));
    fm_assert(erase_if(a, [](StringView s) { return s.back() % 3 == 0; }) == 4);
    fm_assert(a.size() == 6);
    const u32 kept[] = { 1, 2, 4, 5, 7, 8 };
    for (u32 i = 0; i < 6; i++)
        fm_assert(a[i] == long_string(kept[i]));
}

void test_bptr_array(bool growable)
{
    {
        Array<bptr<counted>> a, copies;
        if (growable)
        {
            arrayReserve(a, 10);
            for (int i = 0; i < 10; i++)
                arrayAppend(a, bptr<counted>{InPlace, i});
        }
        else
        {
            a = Array<bptr<counted>>{ValueInit, 10};
            for (int i = 0; i < 10; i++)
                a[i] = bptr<counted>{InPlace, i};
        }
        arrayReserve(copies, 10);
        for (const auto& p : a)
            arrayAppend(copies, p);
        fm_assert(counted_alive == 10);

        fm_assert(erase_if(a, [](const bptr<counted>& p) { return p->x % 2 != 0; }) == 5);
        fm_assert(a.size() == 5);
        for (u32 i = 0; i < 5; i++)
            fm_assert(a[i]->x == int(2 * i) && a[i] == copies[2 * i]);
        for (u32 i = 0; i < 10; i++)
            fm_assert(copies[i].use_count() == (i % 2 == 0 ? 2u : 1u));
        fm_assert(counted_alive == 10);

        arrayClear(copies);
        fm_assert(counted_alive == 5);
        for (const auto& p : a)
            fm_assert(p.use_count() == 1);
    }
    fm_assert(counted_alive == 0);
}

} // namespace

void Test::test_erase_if()
{
    fm_assert(test_cases());
    test_array<u32>();
    test_array<triple>();
    test_non_growable();
    test_strings();
    test_bptr_array(true);
    test_bptr_array(false);
}

} // namespace floormat
