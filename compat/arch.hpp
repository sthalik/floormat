#pragma once

#if defined _MSC_VER
#   ifdef __clang__
#       pragma clang diagnostic push
#       pragma clang diagnostic ignored "-Wreserved-id-macro"
#       pragma clang diagnostic ignored "-Wunused-macros"
#   endif

#   if defined _M_AMD64
#       undef __x86_64__
#       define __x86_64__ 1
#   elif defined _M_IX86
#       undef __i386__
#       define __i386__ 1
#   endif

#   if defined __AVX__ || defined __x86_64__ || \
       defined _M_IX86 && _M_IX86_FP >= 2
#       undef __SSE__
#       undef __SSE2__
#       undef __SSE3__
#       define __SSE__ 1
#       define __SSE2__ 1
#       define __SSE3__ 1
#   endif

#   ifdef __clang__
#       pragma clang diagnostic pop
#   endif
#endif

#if defined __SSE3__
#   define FLOORMAT_ARCH_DENORM_DAZ
#   include <pmmintrin.h>
#elif defined __SSE2__
#   define FLOORMAT_ARCH_DENORM_FTZ
#   include <emmintrin.h>
#endif

#if defined __SSE2__
#   define FLOORMAT_ARCH_FPU_MASK
#   include <xmmintrin.h>
#endif

