#include "app.hpp"
#include "compat/tie.hpp"
#include <cr/Pair.h>
#include <cr/Triple.h>
#include <cr/StaticArray.h>
#include <cr/Pointer.h>
#include <cr/StructuredBindings.h>
#include <mg/Functions.h>
#include <mg/Vector2.h>

namespace floormat {

namespace {

template<typename A, typename B, typename Src>
concept tie2_assignable = requires (A& a, B& b, Src&& src) { tie(a, b) = static_cast<Src&&>(src); };

static_assert(tie2_assignable<int, int, Pair<int, int>>);
static_assert(tie2_assignable<int, int, const Pair<int, int>&>);
static_assert(tie2_assignable<int, int, StaticArray<2, int>>);
static_assert(tie2_assignable<const IgnoreT, int, Pair<float, int>>);
static_assert(!tie2_assignable<int, int, Triple<int, int, int>>);
static_assert(!tie2_assignable<int, int, StaticArray<3, int>>);

constexpr bool test_minmax()
{
    int lo = 0, hi = 0;
    tie(lo, hi) = Math::minmax(31, 7);
    return lo == 7 && hi == 31;
}

constexpr bool test_ignore()
{
    int x = 0, z = 0;
    tie(x, Ignore, z) = Triple<int, float, int>{11, 13.f, 17};
    return x == 11 && z == 17;
}

constexpr bool test_members()
{
    struct { Vector2b offset, bbox_offset; } s = {};
    tie(s.offset, s.bbox_offset) = Pair<Vector2b, Vector2b>{{2, 3}, {-5, -7}};
    return s.offset == Vector2b{2, 3} && s.bbox_offset == Vector2b{-5, -7};
}

constexpr bool test_static_array()
{
    int p = 0, q = 0, r = 0;
    tie(p, q, r) = StaticArray<3, int>{19, 23, 29};
    return p == 19 && q == 23 && r == 29;
}

constexpr bool test_lvalue_source()
{
    const Pair<int, int> src{43, 47};
    int a = 0, b = 0;
    tie(a, b) = src;
    return a == 43 && b == 47 && src.first() == 43 && src.second() == 47;
}

static_assert(test_minmax());
static_assert(test_ignore());
static_assert(test_members());
static_assert(test_static_array());
static_assert(test_lvalue_source());

void test_move()
{
    Pointer<int> ptr;
    int n = 0;
    tie(ptr, n) = Pair<Pointer<int>, int>{Pointer<int>{InPlaceInit, 37}, 41};
    fm_assert(ptr && *ptr == 37 && n == 41);

    auto src = Pair<Pointer<int>, int>{Pointer<int>{InPlaceInit, 53}, 59};
    tie(ptr, n) = move(src);
    fm_assert(ptr && *ptr == 53 && n == 59);
}

} // namespace

void Test::test_tie()
{
    fm_assert(test_minmax());
    fm_assert(test_ignore());
    fm_assert(test_members());
    fm_assert(test_static_array());
    fm_assert(test_lvalue_source());
    test_move();
}

} // namespace floormat
