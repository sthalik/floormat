#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996) // zero-length array
#endif

#include "compat/map.hpp"
#include "compat/assert.hpp"
#include <cr/StaticArray.h>
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
    for (auto i = 0uz; i < array.size(); i++)
        fm_assert(array2[i] == array3[i]);
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

static_assert(test1());
static_assert(test2());
static_assert(test3());
static_assert(test4());
static_assert(test5());

} // namespace

} // namespace floormat
