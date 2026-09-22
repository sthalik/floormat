#include "app.hpp"
#include "compat/format.hpp"
#include <cr/String.h>
#include <cr/StringView.h>

namespace floormat {

namespace {

void check(StringView expected, auto&& fmt, auto&&... args)
{
    char buf[64];
    const auto len = snformat(buf, fmt, args...);
    fm_assert(len == expected.size());
    fm_assert(StringView{buf, len} == expected);
}

void test_stringview()
{
    const auto s = "abc"_s;
    check("abc"_s, "{}"_cf, s);
    check("abc  "_s, "{:<5}"_cf, s);
    check("  abc"_s, "{:>5}"_cf, s);
    check(" abc "_s, "{:^5}"_cf, s);
    check("..abc"_s, "{:.>5}"_cf, s);
    check("ab"_s, "{:.2}"_cf, s);
    check("abc|  xy"_s, "{}|{:>4}"_cf, s, "xy"_s);
}

void test_string()
{
    const auto s = String{"abc"};
    check("abc"_s, "{}"_cf, s);
    check("abc  "_s, "{:<5}"_cf, s);
    check("  abc"_s, "{:>5}"_cf, s);
    check("ab"_s, "{:.2}"_cf, s);
}

} // namespace

namespace Test {

void test_format()
{
    test_stringview();
    test_string();
}

} // namespace Test

} // namespace floormat
