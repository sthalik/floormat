#include "compat/construct-at.hpp"
#include <memory>
#include <type_traits>
#include <utility>

namespace adl_candidates {

struct S { int x; };
int* construct_at(S*, int);
int destroy_at(S*);

} // namespace adl_candidates

namespace floormat {

namespace {

template<typename T, typename... Args>
using construct_at_result = decltype(construct_at(std::declval<T*>(), std::declval<Args>()...));

using adl_candidates::S;
using P = std::pair<int, int>;

static_assert(std::is_same_v<construct_at_result<S, int>, S*>);
static_assert(std::is_same_v<construct_at_result<P, int, int>, P*>);
static_assert(std::is_same_v<decltype(destroy_at(std::declval<S*>())), void>);
static_assert(std::is_same_v<decltype(destroy_at(std::declval<P*>())), void>);

constexpr int construct_inactive_union_member()
{
    union { char c; S s; } u{.c = 0};
    construct_at(&u.s, 7);
    return u.s.x;
}

static_assert(construct_inactive_union_member() == 7);

// MSVC 19.53 can't construct a destroyed aggregate again in constant evaluation.
// std::construct_at and std::destroy_at fail there too.
#if !defined _MSC_VER || defined __clang__
constexpr int construct_after_destroy_at()
{
    union { char c; S s; } u{.c = 0};
    construct_at(&u.s, 7);
    destroy_at(&u.s);
    construct_at(&u.s, 11);
    return u.s.x;
}

static_assert(construct_after_destroy_at() == 11);
#endif

} // namespace

} // namespace floormat
