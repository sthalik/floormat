#pragma once
#ifdef _WIN32
#   define EX_OK        0   /* successful termination */
#   define EX_USAGE     64  /* command line usage error */
#   define EX_DATAERR   65  /* data format error */
#   define EX_SOFTWARE  70  /* internal software error */
#   define EX_CANTCREAT 73  /* can't create (user) output file */
#   define EX_IOERR     74  /* input/output error */
#else
#   include <sysexits.h>
#endif
