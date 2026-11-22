#include "app.hpp"
#include "compat/type-list.hpp"
#include "compat/enum-constant.hpp"
#include "entity/name-of.hpp"
#include "entity/sort-by-name.hpp"
#include <array>
#include <tuple>
#include <type_traits>

namespace floormat::type_list_test {

struct A {};
struct AB {};
struct B {};
struct C {};
template<typename T> struct wrapper {};

enum class sprite_field : u8 { atlas, rotation, frame, delta, COUNT };
enum plain_field { plain_a, plain_b, COUNT };
struct outer { enum class nested_field : u8 { first, second, COUNT }; };
enum class gappy : u8 { first = 0, third = 2, COUNT };

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

static_assert(std::is_same_v<sort_by_name<type_list, A, B, C>, type_list<A, B, C>>);
static_assert(std::is_same_v<sort_by_name<type_list, A, C, B>, type_list<A, B, C>>);
static_assert(std::is_same_v<sort_by_name<type_list, B, A, C>, type_list<A, B, C>>);
static_assert(std::is_same_v<sort_by_name<type_list, B, C, A>, type_list<A, B, C>>);
static_assert(std::is_same_v<sort_by_name<type_list, C, A, B>, type_list<A, B, C>>);
static_assert(std::is_same_v<sort_by_name<type_list, C, B, A>, type_list<A, B, C>>);
static_assert(std::is_same_v<sort_by_name<type_list, const B, A>, type_list<A, const B>>);
static_assert(std::is_same_v<sort_by_name<type_list, AB, B, A>, type_list<A, AB, B>>);
static_assert(std::is_same_v<sort_by_name<type_list, const A, A>, type_list<const A, A>>);
static_assert(std::is_same_v<sort_by_name<type_list, A, const A>, type_list<A, const A>>);
static_assert(std::is_same_v<sort_by_name<type_list>, type_list<>>);
static_assert(std::is_same_v<sort_by_name<type_list, B>, type_list<B>>);
static_assert(std::is_same_v<sort_by_name<std::tuple, C, A>, std::tuple<A, C>>);

using frame_constant = enum_constant<sprite_field, sprite_field::frame>;
using delta_constant = enum_constant<sprite_field, sprite_field::delta>;
static_assert(std::is_same_v<sort_by_name<type_list, frame_constant, delta_constant>,
                             type_list<delta_constant, frame_constant>>);

static_assert(name_of<A> == "floormat::type_list_test::A"_s);
static_assert(is_name_portable<A>);
static_assert(is_name_portable<const A>);
static_assert(is_name_portable<frame_constant>);
static_assert(!is_name_portable<wrapper<A>>);
static_assert(!is_name_portable<type_list<A>>);

using frame_integral_constant = std::integral_constant<sprite_field, sprite_field::frame>;
static_assert(name_of<frame_constant> == "floormat::type_list_test::sprite_field::frame"_s);
static_assert(name_of<frame_integral_constant> == name_of<frame_constant>);
static_assert(enumerator_name<sprite_field::frame> == "frame"_s);
static_assert(enumerator_name<sprite_field::COUNT> == "COUNT"_s);
static_assert(std::is_same_v<std::remove_const_t<decltype(enumerator_names<sprite_field>)>,
                             std::array<StringView, (usize)sprite_field::COUNT>>);
static_assert(enumerator_names<sprite_field> == std::array{"atlas"_s, "rotation"_s, "frame"_s, "delta"_s});

static_assert(name_of<enum_constant<plain_field, plain_b>> == "floormat::type_list_test::plain_b"_s);
static_assert(name_of<std::integral_constant<plain_field, plain_b>> == "floormat::type_list_test::plain_b"_s);
static_assert(enumerator_name<plain_b> == "plain_b"_s);
static_assert(enumerator_names<plain_field>.size() == (usize)plain_field::COUNT);
static_assert(enumerator_names<plain_field> == std::array{"plain_a"_s, "plain_b"_s});

using nested_field = outer::nested_field;
static_assert(name_of<enum_constant<nested_field, nested_field::second>> ==
              "floormat::type_list_test::outer::nested_field::second"_s);
static_assert(name_of<std::integral_constant<nested_field, nested_field::second>> ==
              "floormat::type_list_test::outer::nested_field::second"_s);
static_assert(enumerator_name<nested_field::second> == "second"_s);
static_assert(enumerator_names<nested_field>.size() == (usize)nested_field::COUNT);
static_assert(enumerator_names<nested_field> == std::array{"first"_s, "second"_s});

static_assert(has_enumerator<gappy::first>);
static_assert(has_enumerator<gappy::third>);
static_assert(has_enumerator<gappy::COUNT>);
static_assert(!has_enumerator<(gappy)1>);
static_assert(!has_enumerator<(gappy)200>);
static_assert(name_of<enum_constant<gappy, (gappy)1>>[0] == '(');
static_assert(name_of<std::integral_constant<gappy, (gappy)200>>[0] == '(');
static_assert(enumerator_names<gappy, (gappy)1> == std::array{"first"_s});

} // namespace floormat::type_list_test

namespace floormat {

void Test::test_type_list()
{
    using namespace floormat::type_list_test;
    const auto& names = enumerator_names<sprite_field>;
    fm_assert(names[0] == "atlas"_s);
    fm_assert(names[3] == "delta"_s);
    fm_assert(name_of<enum_constant<nested_field, nested_field::first>> ==
              "floormat::type_list_test::outer::nested_field::first"_s);
    fm_assert(enumerator_name<gappy::third> == "third"_s);
}

} // namespace floormat
