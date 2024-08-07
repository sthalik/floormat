#ifdef __GNUG__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996) // zero-length array
#endif

#include "compat/map.hpp"
#include "compat/assert.hpp"

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

static_assert(test1());
static_assert(test2());

} // namespace

} // namespace floormat
