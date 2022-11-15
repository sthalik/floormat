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

void test_app::test_entity()
{
    static_assert(test_accessors());
    static_assert(test_visitor());
}

} // namespace floormat
