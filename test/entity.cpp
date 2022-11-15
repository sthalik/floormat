#include "app.hpp"
#include "compat/assert.hpp"
#include "src/entity.hpp"
#include <tuple>

using namespace floormat;
using namespace floormat::entities;

struct TestAccessors {
    constexpr int bar() const { return _bar; }
    constexpr void set_bar(int value) { _bar = value; }
    int foo;
    int _bar;
    int _baz;
};

using entity = Entity<TestAccessors>;
static constexpr auto m_foo = entity::type<int>::field{"foo"_s, &TestAccessors::foo, &TestAccessors::foo};
static constexpr auto m_bar = entity::type<int>::field{"bar"_s, &TestAccessors::bar, &TestAccessors::set_bar};
static constexpr auto r_baz = [](const TestAccessors& x) { return x._baz; };
static constexpr auto w_baz = [](TestAccessors& x, int v) { x._baz = v; };
static constexpr auto m_baz = entity::type<int>::field("baz"_s, r_baz, w_baz);

namespace floormat {

static constexpr bool test_accessors()
{
    auto x = TestAccessors{111, 222, 333};

    {
        auto a = m_foo.read(x), b = m_bar.read(x), c = m_baz.read(x);
        fm_assert(a == 111 && b == 222 && c == 333);
    }

    {
        m_foo.write(x, 1111);
        m_bar.write(x, 2222);
        m_baz.write(x, 3333);
        auto a = m_foo.read(x), b = m_bar.read(x), c = m_baz.read(x);
        fm_assert(a == 1111 && b == 2222 && c == 3333);
    }
    return true;
}

static constexpr bool test_visitor()
{
    {
        constexpr auto tuple = std::make_tuple((unsigned char)1, (unsigned short)2, (int)3, (long)4);
        long ret = 0;
        visit_tuple([&](auto x) { ret += (long)x; }, tuple);
        fm_assert(ret == 1 + 2 + 3 + 4);
    }
    {
        int ret = 0;
        visit_tuple([&] { ret++; }, std::tuple<>{});
        fm_assert(ret == 0);
    }
    {
        constexpr auto tuple = std::make_tuple((char)1, (short)2, (long)3);
        static_assert(find_in_tuple([](auto x) { return x == 3; }, tuple));
        static_assert(!find_in_tuple([](auto x) { return x == 5; }, tuple));
    }

    return true;
}

namespace test_sorting {

template<std::size_t I>
struct item {
    static constexpr std::size_t size = I-1;
    std::array<char, I> data;
    consteval item(const char(&str)[I]) {
        std::copy(str, str+I, data.data());
    }
    template<std::size_t J>
    constexpr bool operator==(const item<J>& o) const { return data == o.data; }
};

static constexpr void test()
{
    using namespace floormat::entities::detail;
    constexpr auto tuple = std::make_tuple(item{"bb"}, item{"aaa"}, item{"cccc"}, item{"d"});
    constexpr auto size = std::tuple_size_v<std::decay_t<decltype(tuple)>>;
    constexpr auto key = [](const auto& x) constexpr { return StringView(x.data.data(), x.data.size()); };
    constexpr auto comp = [](auto a, auto b) constexpr { return a < b; };
    using Sort = sort_tuple_<std::decay_t<decltype(tuple)>, key, comp>;
    constexpr auto indices = Sort::sort_indices(tuple, std::make_index_sequence<size>());
    constexpr auto tuple2 = Sort::helper<indices>::do_sort(tuple, std::make_index_sequence<size>());
    static_assert(tuple2 == std::make_tuple(item{"aaa"}, item{"bb"}, item{"cccc"}, item{"d"}));
}

} // namespace test_sorting

void test_app::test_entity()
{
    static_assert(test_accessors());
    static_assert(test_visitor());
    test_sorting::test();
}

namespace type_tests {

using namespace floormat::entities::detail;

template<typename T, typename U> using common_type2 = std::common_type_t<T, U>;
static_assert(std::is_same_v<long long, reduce<common_type2, parameter_pack<char, unsigned short, short, long long>>>);
static_assert(std::is_same_v<parameter_pack<unsigned char, unsigned short, unsigned int>,
                             map<std::make_unsigned_t, parameter_pack<char, short, int>>>);

static_assert(std::is_same_v<parameter_pack<unsigned char, unsigned short, unsigned, unsigned long>,
                             map<std::make_unsigned_t, parameter_pack<char, short, int, long>>>);
static_assert(std::is_same_v<std::tuple<int, short, char>, lift<parameter_pack<short, char>, std::tuple, int>>);
static_assert(std::is_same_v<parameter_pack<long, long long>,
                             skip<3, std::tuple<char, short, int, long, long long>>>);
static_assert(std::is_same_v<parameter_pack<char, short, int>,
                             take<3, std::tuple<char, short, int, float, double, long double>>>);
static_assert(std::is_same_v<int, nth<2, parameter_pack<char, short, int, long, long long>>>);

static_assert(std::is_same_v<parameter_pack<char, short, long, float>,
              except_nth<2, parameter_pack<char, short, int, long, float>>>);

} // namespace type_tests

} // namespace floormat
