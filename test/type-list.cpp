#include "app.hpp"
#include "compat/type-list.hpp"
#include <tuple>
#include <type_traits>

namespace floormat::type_list_test {

struct A {};
struct B {};
struct C {};

static_assert(index_of<A, A, B, C> == 0);
static_assert(index_of<B, A, B, C> == 1);
static_assert(index_of<C, A, B, C> == 2);
static_assert(index_of<A, A> == 0);
static_assert(index_of<const A, A, const A> == 1);

static_assert(is_one_of<A, A, B>);
static_assert(is_one_of<B, A, B>);
static_assert(!is_one_of<C, A, B>);
static_assert(!is_one_of<A>);
static_assert(!is_one_of<const A, A>);

static_assert(is_subset_of<type_list<A>, type_list<A, B>>);
static_assert(is_subset_of<type_list<C, A>, type_list<A, B, C>>);
static_assert(!is_subset_of<type_list<A, B>, type_list<A>>);
static_assert(!is_subset_of<type_list<A, C>, type_list<A, B>>);
static_assert(is_subset_of<type_list<A, B, C>, type_list<A, B, C>>);
static_assert(is_subset_of<type_list<>, type_list<>>);
static_assert(is_subset_of<type_list<>, type_list<A>>);
static_assert(!is_subset_of<type_list<A>, type_list<>>);
static_assert(!is_subset_of<type_list<const A>, type_list<A>>);

static_assert(types_are_distinct<>);
static_assert(types_are_distinct<A>);
static_assert(types_are_distinct<A, B, C>);
static_assert(!types_are_distinct<A, B, A>);
static_assert(!types_are_distinct<A, B, C, C>);
static_assert(types_are_distinct<A, const A>);

static_assert(std::is_same_v<remove_duplicates<type_list, A, B, A, C, B>, type_list<A, B, C>>);
static_assert(std::is_same_v<remove_duplicates<type_list, C, A, B>, type_list<C, A, B>>);
static_assert(std::is_same_v<remove_duplicates<type_list, A, const A, A>, type_list<A, const A>>);
static_assert(std::is_same_v<remove_duplicates<type_list>, type_list<>>);
static_assert(std::is_same_v<remove_duplicates<std::tuple, B, B>, std::tuple<B>>);

} // namespace floormat::type_list_test

namespace floormat {

void Test::test_type_list() {}

} // namespace floormat
