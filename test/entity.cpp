#include "app.hpp"
#include "compat/assert.hpp"
#include "src/entity.hpp"

namespace floormat {

using namespace floormat::entities;

static void test()
{
    struct Test {
        int foo = 111;
        int bar() const { return _bar; }
        void set_bar(int value) { _bar = value; }
        int _baz = 222;
    private:
        int _bar = 333;
    };

    using Entity = entity<Test>;
    constexpr auto m_foo = Entity::Field<int>::make{ "foo"_s, &Test::foo, &Test::foo, };
    constexpr auto m_bar = Entity::Field<int>::make{ "bar"_s, &Test::bar, &Test::set_bar, };
    constexpr auto r_baz = [](const Test& x) { return x._baz; };
    constexpr auto w_baz = [](Test& x, int v) { x._baz = v; };
    constexpr auto m_baz = Entity::Field<int>::make{ "baz"_s, r_baz, w_baz };

    auto x = Test{};
    fm_assert(m_foo.read(x) == 111);
    fm_assert(m_bar.read(x) == 222);
    fm_assert(m_baz.read(x) == 333);

    return true;
}

} // namespace floormat
