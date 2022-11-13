#include "app.hpp"
#include "compat/assert.hpp"
#include "src/entity.hpp"

namespace floormat {

using namespace floormat::entities;

void test_app::test_entity()
{
    struct Test {
        int foo = 111;
        int bar() const { return _bar; }
        void set_bar(int value) { _bar = value; }
        int _baz = 333;
    private:
        int _bar = 222;
    };

    using Entity = entity<Test>;
    constexpr const auto m_foo = Entity::Field<int>::make{ "foo"_s, &Test::foo, &Test::foo, };
    constexpr const auto m_bar = Entity::Field<int>::make{ "bar"_s, &Test::bar, &Test::set_bar, };
    constexpr const auto r_baz = [](const Test& x) { return x._baz; };
    constexpr const auto w_baz = [](Test& x, int v) { x._baz = v; };
    constexpr const auto m_baz = Entity::Field<int>::make{ "baz"_s, r_baz, w_baz };

    auto x = Test{};

    {
        auto a = m_foo.read(x);
        auto b = m_bar.read(x);
        auto c = m_baz.read(x);
        fm_assert(a == 111 && b == 222 && c == 333);
    }

    {
        m_foo.write(x, 1111);
        m_bar.write(x, 2222);
        m_baz.write(x, 3333);
        auto a = m_foo.read(x);
        auto b = m_bar.read(x);
        auto c = m_baz.read(x);
        fm_assert(a == 1111 && b == 2222 && c == 3333);
    }
}

} // namespace floormat
