#include "app.hpp"
#include "compat/debug.hpp"
#include <cerrno>
#include <sstream>
#include <cr/String.h>
#include <cr/StringView.h>
#include <mg/Vector2.h>

namespace floormat {

namespace {

template<typename F>
std::string print(F&& fn, Debug::Flags flags = {})
{
    std::ostringstream out;
    {
        Debug dbg{&out, flags | Debug::Flag::NoNewlineAtTheEnd};
        fn(dbg);
    }
    return out.str();
}

template<typename F>
void check(StringView expected, F&& fn, Debug::Flags flags = {})
{
    const auto str = print(fn, flags);
    fm_assert_equal(expected, StringView{str.data(), str.size()});
}

void test_cat_nospace()
{
    check("x ab y", [](Debug& d) { d << "x" << cat("a", "b") << "y"; });
    check("xab y", [](Debug& d) { d << "x" << nospace("a", "b") << "y"; });
    check("instance: error", [](Debug& d) { d << "instance" << nospace(":") << "error"; });
    check("abc d", [](Debug& d) { d << cat("a", "b", "c") << "d"; });
    check("pos Vector(3, 7), z", [](Debug& d) { d << "pos" << cat(Vector2i{3, 7}, ",") << "z"; });
    check("abcd", [](Debug& d) { d << "a" << cat("b", "c") << "d"; }, Debug::Flag::NoSpace);
}

void test_quoted()
{
    const auto s = String{"lv"};
    check("file 'foo' x", [](Debug& d) { d << "file" << quoted("foo") << "x"; });
    check("x 'lv' y", [&](Debug& d) { d << "x" << quoted(s) << "y"; });
    check("\"a b\"", [](Debug& d) { d << quoted2(String{"a b"}); });
    check("'p', 'q'", [](Debug& d) { d << cat(quoted("p"), ",") << quoted("q"); });
}

void test_error_string()
{
    const auto str = print([](Debug& d) { d << "open" << error_string(EINVAL); });
    const auto view = StringView{str.data(), str.size()};
    fm_assert(view.hasPrefix("open: "_s));
    fm_assert(view.size() > "open: "_s.size());
}

void test_fraction()
{
    check("3.375", [](Debug& d) { d << fraction(3.375f, 3); });
    check("7.5", [](Debug& d) { d << fraction(7.5f); });
    check("13", [](Debug& d) { d << fraction(13.f, 0); });
    check("2.72", [](Debug& d) { d << fraction(2.71828f, 2); });
}

} // namespace

namespace Test {

void test_debug()
{
    test_cat_nospace();
    test_quoted();
    test_error_string();
    test_fraction();
}

} // namespace Test

} // namespace floormat
