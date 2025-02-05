#pragma once
#include "arch.hpp"
#include <cfenv>

#ifdef __MINGW32__
extern "C" __declspec(dllimport) unsigned __cdecl _controlfp(unsigned, unsigned);
#endif

static inline void set_fp_mask()
{
#if defined FLOORMAT_ARCH_DENORM_DAZ
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#elif defined FLOORMAT_ARCH_DENORM_FTZ
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
#endif

#ifdef FLOORMAT_ARCH_FPU_MASK
    _MM_SET_EXCEPTION_MASK(_MM_MASK_MASK);
#endif

#ifdef __APPLE__
#if defined __386__ || defined __x86_64__
    fesetenv(FE_DFL_DISABLE_SSE_DENORMS_ENV);
#elif defined __arm64__
    fesetenv(FE_DFL_DISABLE_DENORMS_ENV);
#endif
#endif

#ifdef _WIN32
#   ifdef __clang__
#       pragma clang diagnostic push
#       pragma clang diagnostic ignored "-Wreserved-id-macro"
#   endif
#   ifndef _DN_FLUSH
#       define _DN_FLUSH 0x01000000
#   endif
#   ifndef _MCW_DN
#       define _MCW_DN 0x03000000
#   endif
#   ifdef __clang__
#       pragma clang diagnostic pop
#   endif
    _controlfp(_DN_FLUSH, _MCW_DN);
#endif
}
