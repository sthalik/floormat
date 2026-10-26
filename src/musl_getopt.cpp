// musl getopt(3) and getopt_long(3), vendored.
//
// Upstream: git://git.musl-libc.org/musl at f21a96538f78fa8e2040831b4209b35f2fb581da
//   src/misc/getopt.c       blob b02b81c34329b3622c8d0eb60c5a4151dc7cb2d2
//   src/misc/getopt_long.c  blob 6949ab1c7eec09753b49946da4ceeef6b89e3adc
//   include/getopt.h        blob 35cbd358b52855fb1596da7970c84c18dd0f1cdb
//
// Changed from upstream, and nothing else. Tabs became four spaces. The two .c files are
// one translation unit, and musl_getopt.h replaces include/getopt.h. _BSD_SOURCE,
// _GNU_SOURCE, <unistd.h>, <getopt.h> and <wchar.h> are gone, because musl_getopt.h
// declares the API and C++ has wchar_t built in. The musl-private includes are gone
// together with what they provided: the __lctrans_cur() message lookup, the FLOCK/FUNLOCK
// pair, and the __getopt_msg() prototype. The globals optarg, optind, opterr, optopt,
// __optpos and __optreset are fields of struct state, which every call takes by
// reference, so separate parses don't share them. Reset a parse with st.optind = 0 or
// st.optreset = 1. Both weak_alias() lines are gone. getopt() takes const char* argv,
// and optarg is const char*. getopt_long() keeps char* const* because it permutes argv.
// Names lost their __ prefix, which C++ reserves. __getopt_msg() is static.
// __getopt_long() is getopt_longʹ(), defined after getopt_long_core(), so the forward
// declaration is gone. The no_argument, required_argument and optional_argument macros
// became enum argument_kind, which is also the type of option::has_arg. Every field of
// struct option has a default. Everything sits in a namespace with C++ linkage because
// MinGW libmingwex exports these same symbols.
//
// ---------------------------------------------------------------------
// Copyright (c) 2005-2020 Rich Felker, et al.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// ---------------------------------------------------------------------

#include "musl_getopt.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

namespace floormat::getopt {

// ---- src/misc/getopt.c -----------------------------------------------

static void getopt_msg(const char *a, const char *b, const char *c, size_t l)
{
    FILE *f = stderr;
    fputs(a, f)>=0
    && fwrite(b, strlen(b), 1, f)
    && fwrite(c, 1, l, f)==l
    && putc('\n', f);
}

int getopt(int argc, const char * const argv[], const char *optstring, state& st)
{
    const auto*& optarg = st.optarg;
    auto &optind = st.optind, &opterr = st.opterr, &optopt = st.optopt;
    auto &optpos = st.optpos, &optreset = st.optreset;

    int i;
    wchar_t c, d;
    int k, l;
    const char *optchar;

    if (!optind || optreset) {
        optreset = 0;
        optpos = 0;
        optind = 1;
    }

    if (optind >= argc || !argv[optind])
        return -1;

    if (argv[optind][0] != '-') {
        if (optstring[0] == '-') {
            optarg = argv[optind++];
            return 1;
        }
        return -1;
    }

    if (!argv[optind][1])
        return -1;

    if (argv[optind][1] == '-' && !argv[optind][2])
        return optind++, -1;

    if (!optpos) optpos++;
    if ((k = mbtowc(&c, argv[optind]+optpos, MB_LEN_MAX)) < 0) {
        k = 1;
        c = 0xfffd; /* replacement char */
    }
    optchar = argv[optind]+optpos;
    optpos += k;

    if (!argv[optind][optpos]) {
        optind++;
        optpos = 0;
    }

    if (optstring[0] == '-' || optstring[0] == '+')
        optstring++;

    i = 0;
    d = 0;
    do {
        l = mbtowc(&d, optstring+i, MB_LEN_MAX);
        if (l>0) i+=l; else i++;
    } while (l && d != c);

    if (d != c || c == ':') {
        optopt = c;
        if (optstring[0] != ':' && opterr)
            getopt_msg(argv[0], ": unrecognized option: ", optchar, k);
        return '?';
    }
    if (optstring[i] == ':') {
        optarg = 0;
        if (optstring[i+1] != ':' || optpos) {
            optarg = argv[optind++];
            if (optpos) optarg += optpos;
            optpos = 0;
        }
        if (optind > argc) {
            optopt = c;
            if (optstring[0] == ':') return ':';
            if (opterr) getopt_msg(argv[0],
                ": option requires an argument: ",
                optchar, k);
            return '?';
        }
    }
    return c;
}

static void permute(char *const *argv, int dest, int src)
{
    char **av = (char **)argv;
    char *tmp = av[src];
    int i;
    for (i=src; i>dest; i--)
        av[i] = av[i-1];
    av[dest] = tmp;
}

static int getopt_long_core(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx, int longonly, state& st)
{
    const auto*& optarg = st.optarg;
    auto &optind = st.optind, &opterr = st.opterr, &optopt = st.optopt;
    optarg = 0;
    if (longopts && argv[optind][0] == '-' &&
        ((longonly && argv[optind][1] && argv[optind][1] != '-') ||
         (argv[optind][1] == '-' && argv[optind][2])))
    {
        int colon = optstring[optstring[0]=='+'||optstring[0]=='-']==':';
        int i, cnt, match;
        char *arg, *opt, *start = argv[optind]+1;
        for (cnt=i=0; longopts[i].name; i++) {
            const char *name = longopts[i].name;
            opt = start;
            if (*opt == '-') opt++;
            while (*opt && *opt != '=' && *opt == *name)
                name++, opt++;
            if (*opt && *opt != '=') continue;
            arg = opt;
            match = i;
            if (!*name) {
                cnt = 1;
                break;
            }
            cnt++;
        }
        if (cnt==1 && longonly && arg-start == mblen(start, MB_LEN_MAX)) {
            int l = arg-start;
            for (i=0; optstring[i]; i++) {
                int j;
                for (j=0; j<l && start[j]==optstring[i+j]; j++);
                if (j==l) {
                    cnt++;
                    break;
                }
            }
        }
        if (cnt==1) {
            i = match;
            opt = arg;
            optind++;
            if (*opt == '=') {
                if (!longopts[i].has_arg) {
                    optopt = longopts[i].val;
                    if (colon || !opterr)
                        return '?';
                    getopt_msg(argv[0],
                        ": option does not take an argument: ",
                        longopts[i].name,
                        strlen(longopts[i].name));
                    return '?';
                }
                optarg = opt+1;
            } else if (longopts[i].has_arg == required_argument) {
                if (!(optarg = argv[optind])) {
                    optopt = longopts[i].val;
                    if (colon) return ':';
                    if (!opterr) return '?';
                    getopt_msg(argv[0],
                        ": option requires an argument: ",
                        longopts[i].name,
                        strlen(longopts[i].name));
                    return '?';
                }
                optind++;
            }
            if (idx) *idx = i;
            if (longopts[i].flag) {
                *longopts[i].flag = longopts[i].val;
                return 0;
            }
            return longopts[i].val;
        }
        if (argv[optind][1] == '-') {
            optopt = 0;
            if (!colon && opterr)
                getopt_msg(argv[0], cnt ?
                    ": option is ambiguous: " :
                    ": unrecognized option: ",
                    argv[optind]+2,
                    strlen(argv[optind]+2));
            optind++;
            return '?';
        }
    }
    return getopt(argc, argv, optstring, st);
}

static int getopt_longʹ(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx, int longonly, state& st)
{
    auto &optind = st.optind, &optpos = st.optpos, &optreset = st.optreset;

    int ret, skipped, resumed;
    if (!optind || optreset) {
        optreset = 0;
        optpos = 0;
        optind = 1;
    }
    if (optind >= argc || !argv[optind]) return -1;
    skipped = optind;
    if (optstring[0] != '+' && optstring[0] != '-') {
        int i;
        for (i=optind; ; i++) {
            if (i >= argc || !argv[i]) return -1;
            if (argv[i][0] == '-' && argv[i][1]) break;
        }
        optind = i;
    }
    resumed = optind;
    ret = getopt_long_core(argc, argv, optstring, longopts, idx, longonly, st);
    if (resumed > skipped) {
        int i, cnt = optind-resumed;
        for (i=0; i<cnt; i++)
            permute(argv, skipped, optind-1);
        optind = skipped + cnt;
    }
    return ret;
}

int getopt_long(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx, state& st)
{
    return getopt_longʹ(argc, argv, optstring, longopts, idx, 0, st);
}

int getopt_long_only(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx, state& st)
{
    return getopt_longʹ(argc, argv, optstring, longopts, idx, 1, st);
}

} // namespace floormat::getopt
