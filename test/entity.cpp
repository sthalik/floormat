#include "app.hpp"
#include "compat/assert.hpp"
#include "entity/metadata.hpp"
#include <tuple>

using namespace floormat;
using namespace floormat::entities;

namespace {

struct TestAccessors {
    constexpr int bar() const { return _bar; }
    constexpr void set_bar(int value) { _bar = value; }
    int foo;
    int _bar;
    int _baz;
};

} // namespace

namespace floormat::entities {

template<> struct entity_accessors<TestAccessors> {
    static constexpr auto accessors()
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
};

} // namespace floormat::entities

namespace {

using entity = Entity<TestAccessors>;
constexpr auto m_foo = entity::type<int>::field{"foo"_s, &TestAccessors::foo, &TestAccessors::foo};
constexpr auto m_bar = entity::type<int>::field{"bar"_s, &TestAccessors::bar, &TestAccessors::set_bar};
constexpr auto r_baz = [](const TestAccessors& x) { return x._baz; };
constexpr auto w_baz = [](TestAccessors& x, int v) { x._baz = v; };
constexpr auto m_baz = entity::type<int>::field("baz"_s, r_baz, w_baz);

} // namespace

namespace floormat {

namespace {

constexpr bool test_accessors()
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

constexpr bool test_visitor()
{
    {
        auto tuple = std::make_tuple((unsigned char)1, (unsigned short)2, (int)3, (long)4);
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

void test_fun2() {
    using entity = Entity<TestAccessors>;
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

void test_erasure() {
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

void test_metadata()
{
    constexpr auto m = entity_metadata<TestAccessors>();
    static_assert(sizeof m == 1);
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

void test_type_name()
{
    struct foobar;
    constexpr StringView name = name_of<foobar>;
    fm_assert(name.contains("foobar"_s));
    static_assert(name.data() == name_of<foobar>.data());
    static_assert(name_of<int> != name_of<unsigned>);
    static_assert(name_of<foobar*> != name_of<const foobar*>);
    using foobar2 = foobar;
    static_assert(name_of<foobar2>.data() == name_of<foobar>.data());
}

[[maybe_unused]] constexpr void test_null_writer()
{
    using entity = Entity<TestAccessors>;
    constexpr auto foo = entity::type<int>::field{"foo"_s, &TestAccessors::foo, nullptr};
    static_assert(foo.writer == nullptr);
    static_assert(!foo.can_write);
    static_assert(std::get<0>(entity_accessors<TestAccessors>::accessors()).can_write);
}

void test_predicate()
{
    using entity = Entity<TestAccessors>;
    constexpr TestAccessors x{0, 0, 0};
    constexpr auto foo = entity::type<int>::field{"foo"_s, &TestAccessors::foo, &TestAccessors::foo,
                                                  [](const TestAccessors&) { return field_status::hidden; }};
    static_assert(foo.is_enabled(x) == field_status::hidden);
    fm_assert(foo.erased().is_enabled(&x) == field_status::hidden);

    foo.erased().do_asserts<TestAccessors>();

    constexpr auto foo2 = entity::type<int>::field{"foo"_s, &TestAccessors::foo, &TestAccessors::foo,
                                                   [](const TestAccessors&) { return field_status::readonly; }};
    static_assert(foo2.is_enabled(x) == field_status::readonly);
    fm_assert(foo2.erased().is_enabled(&x) == field_status::readonly);
    constexpr auto foo3 = entity::type<int>::field{"foo"_s, &TestAccessors::foo, &TestAccessors::foo};
    static_assert(foo3.is_enabled(x) == field_status::enabled);
    fm_assert(foo3.erased().is_enabled(&x) == field_status::enabled);
}

constexpr bool test_names()
{
    constexpr auto m = entity_metadata<TestAccessors>();
    auto [foo1, bar1, baz1] = m.accessors;
    auto [foo2, bar2, baz2] = m.erased_accessors;

    fm_assert(foo1.name == "foo"_s);
    fm_assert(bar1.name == "bar"_s);
    fm_assert(baz1.name == "baz"_s);

    fm_assert(foo2.field_name == "foo"_s);
    fm_assert(bar2.field_name == "bar"_s);
    fm_assert(baz2.field_name == "baz"_s);
    return true;
}

constexpr void test_constraints()
{
    using entity = Entity<TestAccessors>;
    constexpr auto x = TestAccessors{};
    constexpr auto foo = entity::type<int>::field {
        "foo"_s, &TestAccessors::foo, &TestAccessors::foo,
        constantly(constraints::max_length{42}),
        constantly(constraints::range<int>{37, 42}),
        constantly(constraints::group{"foo"_s})
    };

    static_assert(foo.get_range(x) == constraints::range<int>{37, 42});
    static_assert(foo.get_max_length(x) == 42);
    static_assert(foo.get_group(x) == "foo"_s);

    static_assert(m_foo.get_range(x) == constraints::range<int>{});
    static_assert(m_foo.get_max_length(x) == (std::size_t)-1);
    static_assert(m_foo.get_group(x) == ""_s);

    constexpr auto foo2 = entity::type<int>::field {
        "foo"_s, &TestAccessors::foo, &TestAccessors::foo,
        constantly(constraints::max_length {123}),
    };
    static_assert(foo2.get_range(x) == constraints::range<int>{});
    static_assert(foo2.get_max_length(x) == 123);
    static_assert(foo2.get_group(x) == ""_s);
}

void test_erased_constraints()
{
    using entity = Entity<TestAccessors>;
    static constexpr auto foo = entity::type<int>::field{
        "foo"_s, &TestAccessors::foo, &TestAccessors::foo,
        constantly(constraints::max_length{42}),
        constantly(constraints::range<int>{37, 42}),
        constantly(constraints::group{"foo"_s})
    };
    static constexpr auto erased = foo.erased();
    const auto x = TestAccessors{};

    erased.do_asserts<TestAccessors>();
    fm_assert(erased.get_range(&x) == constraints::range<int>{37, 42});
    fm_assert(erased.get_max_length(&x) == 42);
    fm_assert(erased.get_group(&x) == "foo"_s);
}

} // namespace

void test_app::test_entity()
{
    static_assert(test_accessors());
    static_assert(test_visitor());
    test_null_writer();
    static_assert(test_names());
    test_predicate();
    test_fun2();
    test_erasure();
    test_type_name();
    test_metadata();
    test_constraints();
    test_erased_constraints();
}

} // namespace floormat
