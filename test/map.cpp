#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996) // zero-length array
#endif

#include "app.hpp"
#include "compat/map.hpp"
#include "compat/assert.hpp"
#include "compat/zip.hpp"
#include <array>
#include <tuple>
#include <cr/Pair.h>
#include <cr/StaticArray.h>
#include <cr/StructuredBindings.h>
#include <mg/Vector3.h>

namespace floormat {

namespace {

constexpr bool test1()
{
    constexpr auto array = std::array{0, 1, 2, 3, 4};
    auto array2 = map([](int x) constexpr {
        return (unsigned)(x - 1);
    }, array);
    constexpr auto array3 = std::array{(unsigned)-1, 0u, 1u, 2u, 3u};
    fm_assert(array2.size() == array.size());
    fm_assert(array3.size() == array.size());
    for (const auto& [x, y] : zip(array2, array3))
        fm_assert(x == y);
    return true;
}

constexpr bool test2()
{
    fm_assert(map([](int x) constexpr { return x; }, std::array<int, 0>{}).size() == 0);
    return true;
}

constexpr bool test3()
{
    constexpr auto vec = Vector3i{3, 11, 29};

    auto same = map([](int x) constexpr { return x*2; }, vec);
    static_assert(std::is_same_v<decltype(same), Vector3i>);
    fm_assert(same == Vector3i{6, 22, 58});

    auto flt = map([](int x) constexpr { return x * .5f; }, vec);
    static_assert(std::is_same_v<decltype(flt), Vector3>);
    fm_assert(flt == Vector3{1.5f, 5.5f, 14.5f});

    auto dbl = map<Vector3d>([](int x) constexpr { return x + 1; }, vec);
    static_assert(std::is_same_v<decltype(dbl), Vector3d>);
    fm_assert(dbl == Vector3d{4, 12, 30});

    auto nested = map([](int x) constexpr { return std::array{x, -x}; }, vec);
    static_assert(std::is_same_v<decltype(nested), std::array<std::array<int, 2>, 3>>);
    fm_assert(nested[1][0] == 11 && nested[1][1] == -11);

    return true;
}

constexpr bool test4()
{
    constexpr int array[] = { 3, 11, 29, 47 };
    auto array2 = map([](int x) constexpr { return (unsigned)x; }, array);
    static_assert(std::is_same_v<decltype(array2), std::array<unsigned, 4>>);
    fm_assert(array2 == std::array{3u, 11u, 29u, 47u});
    return true;
}

constexpr bool test5()
{
    const auto array = StaticArray<3, int>{InPlaceInit, 3, 11, 29};
    const auto array2 = map([](int x) constexpr { return x*3; }, array);
    static_assert(std::is_same_v<decltype(array2), const StaticArray<3, int>>);
    fm_assert(array2.data()[0] == 9 && array2.data()[1] == 33 && array2.data()[2] == 87);
    return true;
}

constexpr bool test6()
{
    constexpr auto array = std::array{1, 2, 3};
    const auto vecs = map([](int x) constexpr { return Vector2i{x, -x}; }, array);
    static_assert(std::is_same_v<decltype(vecs), const std::array<Vector2i, 3>>);
    fm_assert(vecs[0] == Vector2i{1, -1} && vecs[2] == Vector2i{3, -3});
    return true;
}

constexpr bool test_tuple()
{
    constexpr auto tuple = std::tuple{3, 11u, 29L};
    auto tuple2 = map([](auto x) constexpr { return x*2; }, tuple);
    static_assert(std::is_same_v<decltype(tuple2), std::tuple<int, unsigned, long>>);
    fm_assert(tuple2 == std::tuple{6, 22u, 58L});

    auto tuple3 = map<std::tuple<long, unsigned, int>>([](auto x) constexpr { return x + 1; }, tuple);
    static_assert(std::is_same_v<decltype(tuple3), std::tuple<long, unsigned, int>>);
    fm_assert(tuple3 == std::tuple{4L, 12u, 30});

    auto values = map([](const auto& x) constexpr -> const auto& { return x; }, tuple);
    static_assert(std::is_same_v<decltype(values), std::tuple<int, unsigned, long>>);
    fm_assert(values == tuple);

    unsigned calls = 0;
    auto ordered = map([&](auto x) constexpr { return x + ++calls; }, std::tuple{3u, 11u, 29u});
    fm_assert(ordered == std::tuple{4u, 13u, 32u});
    fm_assert(calls == 3);
    return true;
}

constexpr bool test_empty_tuple()
{
    auto tuple = map([](int x) constexpr { return x; }, std::tuple<>{});
    static_assert(std::is_same_v<decltype(tuple), std::tuple<>>);
    fm_assert(tuple == std::tuple<>{});
    return true;
}

constexpr bool test_tuple_references()
{
    int a = 3;
    unsigned b = 11;
    const std::tuple<int&, const unsigned&> tuple{a, b};
    auto values = map([](auto& x) constexpr -> auto& { return x; }, tuple);
    static_assert(std::is_same_v<decltype(values), std::tuple<int, unsigned>>);
    fm_assert(values == std::tuple{3, 11u});

    auto references = map<std::tuple<int&, const unsigned&>>(
        [](auto& x) constexpr -> auto& { return x; }, tuple);
    static_assert(std::is_same_v<decltype(references), std::tuple<int&, const unsigned&>>);
    fm_assert(&std::get<0>(references) == &a && &std::get<1>(references) == &b);
    std::get<0>(references) = 17;
    b = 29;
    fm_assert(a == 17 && std::get<1>(references) == 29);
    fm_assert(values == std::tuple{3, 11u});
    return true;
}

constexpr bool test_pair()
{
    auto tuple = map([](auto x) constexpr { return x*3; }, std::pair{3, 11u});
    static_assert(std::is_same_v<decltype(tuple), std::tuple<int, unsigned>>);
    fm_assert(tuple == std::tuple{9, 33u});

    auto pair = map([](auto x) constexpr { return x*3; }, Pair<int, unsigned>{3, 11u});
    static_assert(std::is_same_v<decltype(pair), std::tuple<int, unsigned>>);
    fm_assert(pair == std::tuple{9, 33u});
    return true;
}

constexpr bool test_zip_tuple()
{
    auto a = std::array{3};
    auto b = std::array{11u};
    auto zipped = zip(a, b);
    auto tuple = map([](auto x) constexpr { return x*3; }, *zipped.begin());
    static_assert(std::is_same_v<decltype(tuple), std::tuple<int, unsigned>>);
    fm_assert(tuple == std::tuple{9, 33u});
    return true;
}

struct item
{
    int value;
    bool is_kept;
    constexpr bool is_value_odd() const noexcept { return value % 2 != 0; }
};

constexpr bool test_member_pointers()
{
    constexpr item items[] = { { 13, true }, { 14, false }, { 15, false } };
    const auto values = map(&item::value, items);
    static_assert(std::is_same_v<decltype(values), const std::array<int, 3>>);
    fm_assert(values == std::array{13, 14, 15});
    const auto odd = map(&item::is_value_odd, std::array{items[0], items[1], items[2]});
    static_assert(std::is_same_v<decltype(odd), const std::array<bool, 3>>);
    fm_assert(odd == std::array{true, false, true});
    const auto kept = map(&item::is_kept, std::tuple{items[0], items[1]});
    static_assert(std::is_same_v<decltype(kept), const std::tuple<bool, bool>>);
    fm_assert(kept == std::tuple{true, false});
    return true;
}

static_assert(test1());
static_assert(test2());
static_assert(test3());
static_assert(test4());
static_assert(test5());
static_assert(test6());
static_assert(test_tuple());
static_assert(test_empty_tuple());
static_assert(test_tuple_references());
static_assert(test_pair());
static_assert(test_zip_tuple());
static_assert(test_member_pointers());

} // namespace

namespace Test {

void test_map()
{
    volatile int a = 3;
    volatile unsigned b = 11;
    const std::tuple<volatile int&, const volatile unsigned&> tuple{a, b};
    auto values = map([](auto& x) -> auto& { return x; }, tuple);
    static_assert(std::is_same_v<decltype(values), std::tuple<int, unsigned>>);
    fm_assert(values == std::tuple{3, 11u});

    auto references = map<std::tuple<volatile int&, const volatile unsigned&>>(
        [](auto& x) -> auto& { return x; }, tuple);
    static_assert(std::is_same_v<decltype(references), std::tuple<volatile int&, const volatile unsigned&>>);
    fm_assert(&std::get<0>(references) == &a && &std::get<1>(references) == &b);
    std::get<0>(references) = 17;
    b = 29;
    fm_assert(a == 17 && std::get<1>(references) == 29);
    fm_assert(values == std::tuple{3, 11u});
}

} // namespace Test

} // namespace floormat
