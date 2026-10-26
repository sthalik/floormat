#pragma once

namespace floormat::getopt {

enum argument_kind : uint8_t
{
    no_argument, required_argument, optional_argument,
};

struct state
{
    const char *optarg = nullptr;
    int optind = 1, opterr = 1, optopt = 0;
    int optpos = 0, optreset = 0;
};

struct option {
    const char *name = nullptr;
    argument_kind has_arg = no_argument;
    int *flag = nullptr;
    int val = 0;
};

int getopt(int, const char * const [], const char *, state& st);
int getopt_long(int argc, char **argv, const char *optstring, const struct option *longopts, int *idx, state& st);
int getopt_long_only(int argc, char **argv, const char *optstring, const struct option *longopts, int *idx, state& st);

} // namespace floormat::getopt
