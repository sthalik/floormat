#include "app.hpp"
#include "src/musl_getopt.h"
#include "compat/assert.hpp"
#include "compat/array-size.hpp"
#include <climits>
#include <clocale>
#include <cstdlib>
#include <initializer_list>
#include <cr/String.h>
#include <cr/StringView.h>

namespace floormat {

namespace {

using getopt::option;
using getopt::no_argument;
using getopt::required_argument;
using getopt::optional_argument;

// "bet" is a prefix of "beta", so "--bet" checks that an exact match beats a prefix one.
constexpr option longopts[] = {
    { "alpha", no_argument,       nullptr, 'A' },
    { "beta",  required_argument, nullptr, 'B' },
    { "gamma", optional_argument, nullptr, 'G' },
    { "bet",   no_argument,       nullptr, 'T' },
    {},
};

enum class func : uint8_t { getopt, getopt_long, getopt_long_only, };

// One parse. argv[0] is "prog". getopt_long() reorders argv's pointers but never writes
// the strings they point to, so the literals are safe.
struct args
{
    func f;
    const char* optstring;
    const option* longopts;
    char* argv[10] = {};
    int argc = 0;
    int idx = -1;
    getopt::state st;

    args(func f, const char* optstring, std::initializer_list<const char*> list,
         const option* longopts = floormat::longopts) :
        f{f}, optstring{optstring}, longopts{longopts}
    {
        fm_assert(list.size() + 1 < array_size(argv)); // argv[argc] stays null
        argv[argc++] = const_cast<char*>("prog");
        for (const char* s : list)
            argv[argc++] = const_cast<char*>(s);
        // opterr only decides whether getopt_msg() writes to stderr, so clearing it keeps the
        // test silent and changes no result.
        st.opterr = 0;
    }

    int call()
    {
        if (f == func::getopt)
            return getopt::getopt(argc, argv, optstring, st);
        if (f == func::getopt_long)
            return getopt::getopt_long(argc, argv, optstring, longopts, &idx, st);
        return getopt::getopt_long_only(argc, argv, optstring, longopts, nullptr, st);
    }

    void next(int ret, int optind)
    {
        fm_assert_equal(ret, call());
        fm_assert_equal(optind, st.optind);
    }

    void next(int ret, int optind, const char* optarg)
    {
        next(ret, optind);
        if (!optarg)
            fm_assert(!st.optarg);
        else
            fm_assert(st.optarg && StringView{st.optarg} == StringView{optarg});
    }

    void error(int ret, int optind, int optopt)
    {
        next(ret, optind);
        fm_assert_equal(optopt, st.optopt);
    }

    void check_argv(std::initializer_list<const char*> expected) const
    {
        fm_assert_equal((int)expected.size(), argc);
        int i = 0;
        for (const char* s : expected)
            fm_assert(StringView{argv[i++]} == StringView{s});
    }
};

void test_short()
{
    args a{func::getopt, "ab:c::", {"-aa", "-abfoo", "-b", "bar", "-ccc", "-c", "--", "-a"}};
    a.next('a', 1);
    a.st.optreset = 1; // restarts inside "-aa", so optpos must reset too
    a.next('a', 1);
    a.st.optind = 0;
    a.next('a', 1);
    a.next('a', 2);
    a.next('a', 2);
    a.next('b', 3, "foo");
    a.next('b', 5, "bar");
    a.next('c', 6, "cc");
    a.next('c', 7, nullptr);
    a.next(-1, 8);

    args{func::getopt, "a", {}}.next(-1, 1);
    args{func::getopt, "a", {"x", "-a"}}.next(-1, 1);
    args{func::getopt, "a", {"-", "-a"}}.next(-1, 1);
    args b{func::getopt, "a", {}};
    b.argc = 2; // argv[1] is null
    b.next(-1, 1);

    args c{func::getopt, "-a", {"x", "-a", "y"}};
    c.next(1, 2, "x");
    c.next('a', 3);
    c.next(1, 4, "y");
    c.next(-1, 4);

    args d{func::getopt, "+a", {"-a", "x"}};
    d.next('a', 2);
    d.next(-1, 2);
}

void test_short_errors()
{
    args{func::getopt, "a:", {"-x"}}.error('?', 2, 'x');
    args{func::getopt, "a:", {"-:"}}.error('?', 2, ':');
    args{func::getopt, "a:", {"--x"}}.error('?', 1, '-');
    args{func::getopt, ":a", {"-x"}}.error('?', 2, 'x');
    args{func::getopt, "a:", {"-a"}}.error('?', 3, 'a');
    args{func::getopt, ":a:", {"-a"}}.error(':', 3, 'a');

    args a{func::getopt, "a:", {"-x", "-a"}};
    a.error('?', 2, 'x');
    a.error('?', 4, 'a');
}

bool rejects_0xff()
{
    wchar_t c;
    const bool ret = std::mbtowc(&c, "\xff", MB_LEN_MAX) < 0;
    std::mbtowc(nullptr, nullptr, 0);
    return ret;
}

// getopt() substitutes U+FFFD where mbtowc() rejects a byte. The C locale accepts 0xff on
// Windows and macOS, so this tries the UTF-8 locales, and skips the check when none exists.
void test_invalid_multibyte()
{
    const String old{std::setlocale(LC_CTYPE, nullptr)};
    bool rejects = rejects_0xff();
    for (const char* name : { "C.UTF-8", ".UTF-8", "en_US.UTF-8" })
    {
        if (rejects)
            break;
        if (std::setlocale(LC_CTYPE, name))
            rejects = rejects_0xff();
    }
    if (rejects)
        args{func::getopt, "a", {"-\xff"}}.error('?', 2, 0xfffd);
    std::setlocale(LC_CTYPE, old.data());
}

void test_long()
{
    args a{func::getopt_long, "", {"--alpha", "--al", "--bet", "--beta=x", "--beta", "y", "--gamma=z", "--gamma"}};
    a.next('A', 2);
    fm_assert_equal(0, a.idx);
    a.st.optreset = 1;
    a.next('A', 2);
    a.st.optind = 0;
    a.next('A', 2);
    a.next('A', 3);
    a.next('T', 4);
    fm_assert_equal(3, a.idx);
    a.next('B', 5, "x");
    a.next('B', 7, "y");
    a.next('G', 8, "z");
    a.next('G', 9, nullptr);
    a.next(-1, 9);

    int flag = 0;
    const option flags[] = { { "flag", no_argument, &flag, 'F' }, {} };
    args{func::getopt_long, "", {"--flag"}, flags}.next(0, 2);
    fm_assert_equal('F', flag);

    args{func::getopt_long, "a", {"-a"}, nullptr}.next('a', 2);
}

void test_long_errors()
{
    args{func::getopt_long, "", {"--alpha=1"}}.error('?', 2, 'A');
    args{func::getopt_long, ":", {"--alpha=1"}}.error('?', 2, 'A');
    args{func::getopt_long, "", {"--beta"}}.error('?', 2, 'B');
    args{func::getopt_long, ":", {"--beta"}}.error(':', 2, 'B');
    args{func::getopt_long, "+:", {"--beta"}}.error(':', 2, 'B');
    args{func::getopt_long, "-:", {"--beta"}}.error(':', 2, 'B');
    args{func::getopt_long, "", {"--be"}}.error('?', 2, 0);
    args{func::getopt_long, "", {"--zeta"}}.error('?', 2, 0);
    args{func::getopt_long, ":", {"--zeta"}}.error('?', 2, 0);

    args a{func::getopt_long, "", {"--alpha=1", "--zeta", "--beta"}};
    a.error('?', 2, 'A');
    a.error('?', 3, 0);
    a.error('?', 4, 'B');
}

void test_permute()
{
    args a{func::getopt_long, "ab:", {"x", "-ab", "1", "-", "--alpha", "z"}};
    a.next('a', 1);
    a.next('b', 3, "1");
    a.next('A', 4);
    a.next(-1, 4);
    a.check_argv({"prog", "-ab", "1", "--alpha", "x", "-", "z"});

    args b{func::getopt_long, "a", {"x", "--", "-a"}};
    b.next(-1, 2);
    b.check_argv({"prog", "--", "x", "-a"});

    args{func::getopt_long, "+a", {"x", "-a"}}.next(-1, 1);

    args c{func::getopt_long, "-a", {"x", "-a"}};
    c.next(1, 2, "x");
    c.next('a', 3);
    c.next(-1, 3);

    args d{func::getopt_long, "", {}};
    d.argc = 2; // argv[1] is null
    d.next(-1, 1);
    args e{func::getopt_long, "", {"x"}};
    e.argc = 3; // the search for an option stops at the null argv[2]
    e.next(-1, 1);
}

// A single-dash word parses as short options when it matches no long option, or when it is
// one letter that is also a short option.
void test_long_only()
{
    args a{func::getopt_long_only, "ab:", {"-alpha", "-al", "-a", "-g", "--beta=x", "-xyz"}};
    a.next('A', 2);
    a.next('A', 3);
    a.next('a', 4);
    a.next('G', 5, nullptr);
    a.next('B', 6, "x");
    a.error('?', 6, 'x');

    args{func::getopt_long_only, "+", {"-"}}.next(-1, 1);
}

} // namespace

void Test::test_getopt()
{
    test_short();
    test_short_errors();
    test_invalid_multibyte();
    test_long();
    test_long_errors();
    test_permute();
    test_long_only();
}

} // namespace floormat
