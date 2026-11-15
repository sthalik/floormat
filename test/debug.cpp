#include "app.hpp"
#include "compat/debug.hpp"
#include "compat/nanosecond.hpp"
#include "compat/strerror.hpp"
#include "src/intra-coord.hpp"
#include "src/point.hpp"
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

void test_cat()
{
    auto s = String{"s"};
    const int n = 5;
    check("x ab y", [](Debug& d) { d << "x" << cat("a", "b") << "y"; });
    check("abc d", [](Debug& d) { d << cat("a", "b", "c") << "d"; });
    check("a b c", [](Debug& d) { d << "a" << cat("b") << "c"; });
    check("ab cd", [](Debug& d) { d << cat("a", "b") << cat("c", "d"); });
    check("dt 42ms -3px x=1.5", [](Debug& d) { d << "dt" << cat(42, "ms") << cat(-3, "px") << cat("x=", 1.5f); });
    check("pos Vector(3, 7), z", [](Debug& d) { d << "pos" << cat(Vector2i{3, 7}, ",") << "z"; });
    check("abc d", [](Debug& d) { d << "a" << Debug::nospace << cat("b", "c") << "d"; });
    check("x abc", [](Debug& d) { d << "x" << cat(cat("a", "b"), "c"); });
    check("s5tmp!", [&](Debug& d) { d << cat(s, n, String{"tmp"}, "!"); });
    check("abcd", [](Debug& d) { d << "a" << cat("b", "c") << "d"; }, Debug::Flag::NoSpace);
}

void test_nospace()
{
    check("xab y", [](Debug& d) { d << "x" << nospace("a", "b") << "y"; });
    check("instance: error", [](Debug& d) { d << "instance" << nospace(":") << "error"; });
    check("a b", [](Debug& d) { d << nospace("a") << "b"; });
    check("x5 y", [](Debug& d) { d << "x" << nospace(5) << "y"; });
    check("vVector(1, 2)", [](Debug& d) { d << "v" << nospace(Vector2i{1, 2}); });
    check("xab", [](Debug& d) { d << "x" << nospace(cat("a", "b")); });
    check("xab", [](Debug& d) { d << "x" << cat(nospace("a"), "b"); });
    check("abc", [](Debug& d) { d << "a" << nospace("b") << "c"; }, Debug::Flag::NoSpace);
}

void test_quoted()
{
    const auto s = String{"lv"};
    auto m = String{"mut"};
    check("file 'foo' x", [](Debug& d) { d << "file" << quoted("foo") << "x"; });
    check("x 'lv' 'mut' y", [&](Debug& d) { d << "x" << quoted(s) << quoted(m) << "y"; });
    check("'a' b", [](Debug& d) { d << quoted("a") << "b"; });
    check("''", [](Debug& d) { d << quoted(""); });
    check("x `y` z", [](Debug& d) { d << "x" << quoted("y", '`') << "z"; });
    check("\"a b\"", [](Debug& d) { d << quoted2(String{"a b"}); });
    check("'42' 'Vector(1, 2)'", [](Debug& d) { d << quoted(42) << quoted(Vector2i{1, 2}); });
    check("'p', 'q'", [](Debug& d) { d << cat(quoted("p"), ",") << quoted("q"); });
    check("x'a'", [](Debug& d) { d << "x" << nospace(quoted("a")); });
    check("a'b'c", [](Debug& d) { d << "a" << quoted("b") << "c"; }, Debug::Flag::NoSpace);
}

void test_error_string()
{
    char buf1[256], buf2[256];
    const auto inval = String{get_error_string(buf1, EINVAL)};
    const auto noent = String{get_error_string(buf2, ENOENT)};
    fm_assert(!inval.isEmpty());
    fm_assert(StringView{inval} != StringView{noent});

    check("open: "_s + inval + " x"_s, [](Debug& d) { d << "open" << error_string(EINVAL) << "x"; });
    check(": "_s + inval, [](Debug& d) { d << error_string(EINVAL); });
    check("open:"_s + inval, [](Debug& d) { d << "open" << error_string(EINVAL); }, Debug::Flag::NoSpace);

    errno = ENOENT;
    const auto saved = error_string();
    errno = 0;
    check("open: "_s + noent, [&](Debug& d) { d << "open" << saved; });
}

void test_fraction()
{
    check("3.375", [](Debug& d) { d << fraction(3.375f, 3); });
    check("7.5", [](Debug& d) { d << fraction(7.5f); });
    check("1.3", [](Debug& d) { d << fraction(1.26f); });
    check("13", [](Debug& d) { d << fraction(13.f, 0); });
    check("2.72", [](Debug& d) { d << fraction(2.71828f, 2); });
    check("0.00", [](Debug& d) { d << fraction(0.f, 2); });
    check("-1.25", [](Debug& d) { d << fraction(-1.25f, 2); });
    check("12345.7", [](Debug& d) { d << fraction(12345.678f); });
    check("a 1.5 b", [](Debug& d) { d << "a" << fraction(1.5f) << "b"; });
    check("1.5ms", [](Debug& d) { d << cat(fraction(1.5f), "ms"); });
}

void test_ns()
{
    check("dt: 37ₙₛ", [](Debug& d) { d << "dt:" << Ns{37}; });
    check("dt: 503ₙₛ", [](Debug& d) { d << "dt:" << Ns{503}; });
    check("dt: 999ₙₛ", [](Debug& d) { d << "dt:" << Ns{999}; });
    check("dt: 1.000ᵤₛ", [](Debug& d) { d << "dt:" << Ns{1000}; });
    check("dt: 2.003ᵤₛ", [](Debug& d) { d << "dt:" << Ns{2003}; });
    check("dt: 49.999ᵤₛ", [](Debug& d) { d << "dt:" << Ns{49'999}; });
    check("dt: 0.0500ₘₛ", [](Debug& d) { d << "dt:" << Ns{50'000}; });
    check("dt: 12.3457ₘₛ", [](Debug& d) { d << "dt:" << Ns{12'345'678}; });
    check("dt: 100.000ₘₛ", [](Debug& d) { d << "dt:" << Ns{100'000'000}; });
    check("dt: 123.457ₘₛ", [](Debug& d) { d << "dt:" << Ns{123'456'789}; });
    check("dt: 1.00ₛ", [](Debug& d) { d << "dt:" << Ns{1'000'000'000}; });
    check("dt: 1.50ₛ", [](Debug& d) { d << "dt:" << Ns{1'500'000'000}; });
    check("dt: 10.0ₛ", [](Debug& d) { d << "dt:" << Ns{10'000'000'000}; });
    check("dt: 12.3ₛ", [](Debug& d) { d << "dt:" << Ns{12'345'678'901}; });
}

void test_coord_printers()
{
    using wrapping_coord = basic_intra_coord<intra_coord::Wrapping>;
    check("chunk{3,-7}", [](Debug& d) { d << chunk_coords{3, -7}; });
    check("chunk{11,-13}", [](Debug& d) { d << chunk_coords_{11, -13, 0}; });
    check("chunk{17,-19,-1}", [](Debug& d) { d << chunk_coords_{17, -19, -1}; });
    check("intra_coord{5,701}", [](Debug& d) { d << intra_coord{Vector2i{5, 701}}; });
    check("intra_coord<Wrapping>{-1,1103}", [](Debug& d) { d << wrapping_coord{Vector2i{-1, 1103}}; });
    check("point{{1,2,3},{4,5},{6,7}}", [](Debug& d) { d << point{{1, 2, 3}, {4, 5}, {6, 7}}; });
    check("point{{-1,0,0},{15,15},{31,-32}}", [](Debug& d) { d << point{{-1, 0, 0}, {15, 15}, {31, -32}}; });
    check("at point{{1,2,3},{4,5},{6,7}} x", [](Debug& d) { d << "at" << point{{1, 2, 3}, {4, 5}, {6, 7}} << "x"; });
}

} // namespace

namespace Test {

void test_debug()
{
    test_cat();
    test_nospace();
    test_quoted();
    test_error_string();
    test_fraction();
    test_ns();
    test_coord_printers();
}

} // namespace Test

} // namespace floormat
