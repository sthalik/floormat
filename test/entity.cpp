#include "app.hpp"
#include "compat/assert.hpp"
#include "entity/entity.hpp"
#include <tuple>

using namespace floormat;
using namespace floormat::entities;

struct TestAccessors {
    constexpr int bar() const { return _bar; }
    constexpr void set_bar(int value) { _bar = value; }
    int foo;
    int _bar;
    int _baz;

    static constexpr auto accessors() noexcept;
};

constexpr auto TestAccessors::accessors() noexcept
{
    using entity = Entity<TestAccessors>;
    constexpr auto r_baz = [](const TestAccessors& x) { return x._baz; };
    constexpr auto w_baz = [](TestAccessors& x, int v) { x._baz = v; };

    constexpr auto tuple = std::make_tuple(
        entity::type<int>::field{"foo"_s, &TestAccessors::foo, &TestAccessors::foo},
        entity::type<int>::field{"bar"_s, &TestAccessors::bar, &TestAccessors::set_bar},
        entity::type<int>::field("baz"_s, r_baz, w_baz)
    );
    return tuple;
}

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

static void test_fun2() {
    static constexpr auto read_fn = [](const TestAccessors& x) constexpr { return x.bar(); };
    static constexpr auto write_fn = [](TestAccessors& x, int value) constexpr { x.set_bar(value); };
    constexpr auto read_bar = fu2::function_view<int(const TestAccessors&) const>{read_fn};
    constexpr auto write_bar = fu2::function_view<void(TestAccessors&, int) const>{write_fn};
    constexpr auto m_bar2 = entity::type<int>::field{"bar"_s, read_bar, write_bar};

    auto x = TestAccessors{1, 2, 3};
    fm_assert(m_bar2.read(x) == 2);
    m_bar2.write(x, 22);
    fm_assert(m_bar2.read(x) == 22);
}

static void test_erasure() {
    erased_accessor accessors[] = {
        m_foo.erased(),
        m_bar.erased(),
        m_baz.erased(),
    };
    auto obj = TestAccessors{1, 2, 3};
    int value = 0;
    accessors[1].read_fun(&obj, accessors[1].reader, &value);
    fm_assert(value == 2);
    int value2 = 22222, value2_ = 0;
    accessors[1].write_fun(&obj, accessors[1].writer, &value2);
    accessors[1].read_fun(&obj, accessors[1].reader, &value2_);
    fm_assert(value2 == value2_);
}

static void test_metadata()
{
    constexpr auto m = entity_metadata<TestAccessors>();
    fm_assert(m.class_name == name_of<TestAccessors>);
    fm_assert(m.class_name.contains("TestAccessors"_s));
    const auto [foo, bar, baz] = m.accessors;
    const auto [foo2, bar2, baz2] = m.erased_accessors;
    TestAccessors x{0, 0, 0};
    foo.write(x, 1);
    fm_assert(x.foo == 1);
    int bar_ = 2;
    bar2.write_fun(&x, bar2.writer, &bar_);
    fm_assert(x.bar() == 2);
    baz2.write<TestAccessors, int>(x, 3);
    fm_assert(baz2.read<TestAccessors, int>(x) == 3);
}

static void test_type_name()
{
    using namespace entities;
    struct foobar;
    constexpr StringView name = name_of<foobar>;
    fm_assert(name.contains("foobar"_s));
    static_assert(name.data() == name_of<foobar>.data());
    static_assert(name_of<int> != name_of<unsigned>);
    static_assert(name_of<foobar*> != name_of<const foobar*>);
}

void test_app::test_entity()
{
    static_assert(test_accessors());
    static_assert(test_visitor());
    test_fun2();
    test_erasure();
    test_type_name();
    test_metadata();
}

} // namespace floormat
