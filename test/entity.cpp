#include "app.hpp"
#include "compat/assert.hpp"
#include "compat/limits.hpp"
#include "compat/function-view.hpp"
#include "compat/exception.inl"
#include "entity/metadata.hpp"
#include "entity/accessor.hpp"
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

struct base_fields
{
    int _qux = 0;
    constexpr int qux() const noexcept { return _qux; }
    constexpr void set_qux(int value) noexcept { _qux = value; }
};

struct WidenedAccessors : base_fields
{
    int _bar = 0;
    const int _fixed = 5;
    constexpr const int& bar_ref() const noexcept { return _bar; }
    constexpr void set_bar_ref(const int& value) { _bar = value; }
};

struct unrelated {};

} // namespace

namespace floormat::entities {

template<> struct entity_accessors<TestAccessors, inspect_intent_t> {
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
    constexpr auto read_bar = function_view<int(const TestAccessors&) const>{read_fn};
    constexpr auto write_bar = function_view<void(TestAccessors&, int) const>{write_fn};
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
    constexpr auto m = entity_metadata<TestAccessors, inspect_intent_t>();
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
#ifndef __CLION_IDE__
    static_assert(name_of<int> != name_of<unsigned>);
    static_assert(name_of<foobar*> != name_of<const foobar*>);
#endif
    using foobar2 = foobar;
    static_assert(name_of<foobar2>.data() == name_of<foobar>.data());
}

[[maybe_unused]] constexpr void test_null_writer()
{
    using entity = Entity<TestAccessors>;
    constexpr auto foo = entity::type<int>::field{"foo"_s, &TestAccessors::foo, nullptr};
    static_assert(foo.writer == nullptr);
    static_assert(!foo.can_write);
    static_assert(std::get<0>(entity_accessors<TestAccessors, inspect_intent_t>::accessors()).can_write);
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

constexpr bool test_member_pointer_kinds()
{
    using W = WidenedAccessors;
    using entity = Entity<W>;
    static_assert(FieldReader<decltype(&W::bar_ref), W, int>);
    static_assert(FieldReader<int base_fields::*, W, int>);
    static_assert(FieldReader<decltype(&base_fields::qux), W, int>);
    static_assert(FieldReader<const int W::*, W, int>);
    static_assert(!FieldReader<int (W::*)(), W, int>);
    static_assert(!FieldReader<void (W::*)(int), W, int>);
    static_assert(!FieldReader<decltype(&W::bar_ref), W, unrelated>);
    static_assert(FieldWriter<decltype(&W::set_bar_ref), W, int>);
    static_assert(FieldWriter<int base_fields::*, W, int>);
    static_assert(FieldWriter<decltype(&base_fields::set_qux), W, int>);
    static_assert(FieldWriter<std::nullptr_t, W, int>);
    static_assert(!FieldWriter<const int W::*, W, int>);
    static_assert(!FieldWriter<decltype(&W::bar_ref), W, int>);
    static_assert(!FieldWriter<int (*)(W&, int), W, int>);
    static_assert(!FieldWriter<int W::*, W, unrelated>);

    constexpr auto m_ref = entity::type<int>::field{"ref"_s, &W::bar_ref, &W::set_bar_ref};
    constexpr auto m_ptr = entity::type<int>::field{"qux"_s, &base_fields::_qux, &base_fields::_qux};
    constexpr auto m_fn = entity::type<int>::field{"qux"_s, &base_fields::qux, &base_fields::set_qux};
    constexpr auto m_fixed = entity::type<int>::field{"fixed"_s, &W::_fixed, nullptr};
    W x;
    x._bar = 101;
    x._qux = 103;
    fm_assert(m_ref.read(x) == 101 && m_ptr.read(x) == 103 && m_fn.read(x) == 103 && m_fixed.read(x) == 5);
    m_ref.write(x, 107);
    m_ptr.write(x, 109);
    fm_assert(x._bar == 107 && x._qux == 109);
    m_fn.write(x, 113);
    fm_assert(x._qux == 113 && m_ptr.read(x) == 113);
    return true;
}

constexpr bool test_names()
{
    constexpr auto m = entity_metadata<TestAccessors, inspect_intent_t>();
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
    };

    static_assert(foo.get_range(x) == constraints::range<int>{37, 42});
    static_assert(foo.get_max_length(x) == 42);

    static_assert(m_foo.get_range(x) == constraints::range<int>{});
    static_assert(m_foo.get_max_length(x) == (usize)-1);

    constexpr auto foo2 = entity::type<int>::field {
        "foo"_s, &TestAccessors::foo, &TestAccessors::foo,
        constantly(constraints::max_length {123}),
    };
    static_assert(foo2.get_range(x) == constraints::range<int>{});
    static_assert(foo2.get_max_length(x) == 123);
}

void test_erased_constraints()
{
    using entity = Entity<TestAccessors>;
    static constexpr auto foo = entity::type<int>::field{
        "foo"_s, &TestAccessors::foo, &TestAccessors::foo,
        constantly(constraints::max_length{42}),
        constantly(constraints::range<int>{37, 42}),
    };
    static constexpr auto erased = foo.erased();
    const auto x = TestAccessors{};

    erased.do_asserts<TestAccessors>();
    fm_assert(erased.get_range(&x) == constraints::range<int>{37, 42});
    fm_assert(erased.get_max_length(&x) == 42);
}

constexpr bool test_range1()
{
    constexpr auto x = TestAccessors{};
    constexpr auto r = m_foo.get_range(x);
    static_assert(r.max == limits<int>::max);
    return true;
}

void test_range2()
{
    constexpr auto x = TestAccessors{};
    constexpr auto rʹ = m_foo.get_range(x);
    const auto A = m_foo.erased();
    auto r = A.get_range(&x);
    auto i = r.convert<int>();
    fm_assert(i.second() == rʹ.max);
}

constexpr bool test_range3()
{
    constexpr auto f = entity::type<Vector2i>::field("vec"_s, constantly(Vector2i(0)), [](auto&&, auto&&) {});
    constexpr auto r = f.get_range(f.range, TestAccessors{});
    static_assert(r.max == Vector2i(limits<int>::max));

    return true;
}

void test_range4()
{
    constexpr auto x = TestAccessors{};
    constexpr auto f = entity::type<Vector2i>::field("vec"_s, constantly(Vector2i(0)), [](auto&&, auto&&) {});
    constexpr auto rʹ = f.get_range(f.range, TestAccessors{});
    const auto A = f.erased();
    const auto r = A.get_range(&x).convert<Vector2i>();
    fm_assert(r.first() == rʹ.min);
    fm_assert(r.second() == rʹ.max);
}

constexpr bool test_enum_range()
{
    enum class E { foo, bar };
    constexpr auto x = TestAccessors{};
    constexpr auto f = entity::type<E>::field("vec"_s, constantly(E::bar), [](auto&&, auto&&) {});
    static_assert(f.read(x) == E::bar);
    constexpr auto r = f.get_range(x);
    static_assert(r.max != E{0});
    return true;
}

void test_unsigned_range()
{
    using range = erased_constraints::range;
    range r{.min{.i = 0}, .max{.i = 255}, .type = range::type_int};
    const auto a = r.convert<u8>();
    fm_assert(a.first() == 0 && a.second() == 255);
    r.max.i = 65535;
    const auto b = r.convert<u16>();
    fm_assert(b.first() == 0 && b.second() == 65535);
    r.max.i = 4294967295LL;
    const auto c = r.convert<u32>();
    fm_assert(c.first() == 0 && c.second() == 4294967295u);

    r.type = range::type_int4;
    r.min.i4 = {0, 17, 19, 23};
    r.max.i4 = {255, 253, 251, 239};
    const auto d = r.convert<Vector2ub>();
    fm_assert(d.first() == Vector2ub{0, 17} && d.second() == Vector2ub{255, 253});
    const auto e = r.convert<Vector3ub>();
    fm_assert(e.first() == Vector3ub{0, 17, 19} && e.second() == Vector3ub{255, 253, 251});
    const auto f = r.convert<Vector4ub>();
    fm_assert(f.first() == Vector4ub{0, 17, 19, 23} && f.second() == Vector4ub{255, 253, 251, 239});
}

template<typename T>
void assert_convert_throws(const erased_constraints::range& r, StringView message)
{
    bool caught = false;
    try
    {
        (void)r.convert<T>();
    }
    catch (const floormat::exception& e)
    {
        caught = true;
        fm_assert(StringView{e.what()} == message);
    }
    fm_assert(caught);
}

void test_unsigned_range_overflow()
{
    using range = erased_constraints::range;
    range r{.min{.i = 256}, .max{.i = 255}, .type = range::type_int};
    assert_convert_throws<u8>(r, "checked_cast: 256 is out of range [0, 255]"_s);
    r.min.i = 0;
    r.max.i = 257;
    assert_convert_throws<u8>(r, "checked_cast: 257 is out of range [0, 255]"_s);

    r.type = range::type_int4;
    r.min.i4 = {0, 258, 19, 23};
    r.max.i4 = {255, 253, 251, 239};
    assert_convert_throws<Vector4ub>(r, "checked_cast: 258 is out of range [0, 255]"_s);
    r.min.i4 = {0, 17, 19, 23};
    r.max.i4 = {255, 253, 259, 239};
    assert_convert_throws<Vector4ub>(r, "checked_cast: 259 is out of range [0, 255]"_s);
}

} // namespace

void Test::test_entity()
{
    static_assert(test_accessors());
    static_assert(test_visitor());
    test_null_writer();
    static_assert(test_names());
    static_assert(test_member_pointer_kinds());
    test_predicate();
    test_fun2();
    test_erasure();
    test_type_name();
    test_metadata();
    test_constraints();
    test_erased_constraints();
    static_assert(test_range1());
    test_range2();
    static_assert(test_range3());
    test_range4();
    static_assert(test_enum_range());
    test_unsigned_range();
    test_unsigned_range_overflow();
}

} // namespace floormat
