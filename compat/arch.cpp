#include "arch.hpp"

#if defined __x86_64__ || defined _M_X64 || defined __SSE__ || defined _M_IX86_FP && _M_IX86_FP >= 1
#   define FLOORMAT_FP_MXCSR
#   include <pmmintrin.h>
#elif defined _MSC_VER && (defined _M_ARM64 || defined _M_ARM)
#   define FLOORMAT_FP_CONTROLFP
#   include <float.h>
#endif

namespace floormat {

void set_fp_mask() noexcept
{
#if defined FLOORMAT_FP_MXCSR
    uint32_t bits = _MM_MASK_MASK | _MM_FLUSH_ZERO_ON;
    // Setting DAZ faults on a CPU without it. Every x86-64 or SSE3 CPU has it.
#   if defined __x86_64__ || defined _M_X64 || defined __SSE3__ || defined __AVX__
    bits |= _MM_DENORMALS_ZERO_ON;
#   endif
    _mm_setcsr(_mm_getcsr() | bits);
#elif defined FLOORMAT_FP_CONTROLFP
    (void)_controlfp(_DN_FLUSH, _MCW_DN);
// ARM's FZ bit flushes denormal inputs as well as results.
#elif defined __aarch64__ || defined __arm64__
    uint64_t fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | uint64_t{1} << 24));
#elif defined __arm__ && defined __ARM_FP
    uint32_t fpscr;
    __asm__ volatile("vmrs %0, fpscr" : "=r"(fpscr));
    __asm__ volatile("vmsr fpscr, %0" : : "r"(fpscr | uint32_t{1} << 24));
#endif
}

namespace {

struct set_fp_mask_at_startup final
{
    set_fp_mask_at_startup() noexcept { set_fp_mask(); }
};

[[maybe_unused]] const set_fp_mask_at_startup fp_mask_at_startup;

} // namespace

} // namespace floormat
