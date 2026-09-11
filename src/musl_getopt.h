#pragma once

namespace floormat::getopt {

int getopt(int, char * const [], const char *);
extern char *optarg;
extern int optind, opterr, optopt;
extern int optpos, optreset;

} // namespace floormat::getopt
