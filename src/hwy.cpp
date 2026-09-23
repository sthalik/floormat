#include "hwy.hpp"
#include <cstddef>
#include <hwy/contrib/sort/vqsort.h>

namespace floormat {

#define FM_SAME_LAYOUT(T, a, b)                                                                         \
    static_assert(sizeof(T) == sizeof(hwy::T) && alignof(T) == alignof(hwy::T));                        \
    static_assert(offsetof(T, a) == offsetof(hwy::T, a) && offsetof(T, b) == offsetof(hwy::T, b))
FM_SAME_LAYOUT(uint128_t, lo, hi);
FM_SAME_LAYOUT(K64V64, value, key);
FM_SAME_LAYOUT(K32V32, value, key);
#undef FM_SAME_LAYOUT

// vqsort casts keys to its lane type on entry and never reads them through H, so the struct cast is safe.
#define FM_VQSORT(T, H)                                                                                 \
    void vqsort(T* keys, uint32_t n, sort_order order)                                                  \
    {                                                                                                   \
        if (order == sort_order::ascending)                                                             \
            hwy::VQSort((H*)keys, n, hwy::SortAscending());                                             \
        else                                                                                            \
            hwy::VQSort((H*)keys, n, hwy::SortDescending());                                            \
    }                                                                                                   \
    void vqsort_partial(T* keys, uint32_t n, uint32_t k, sort_order order)                              \
    {                                                                                                   \
        if (order == sort_order::ascending)                                                             \
            hwy::VQPartialSort((H*)keys, n, k, hwy::SortAscending());                                   \
        else                                                                                            \
            hwy::VQPartialSort((H*)keys, n, k, hwy::SortDescending());                                  \
    }                                                                                                   \
    void vqselect(T* keys, uint32_t n, uint32_t k, sort_order order)                                    \
    {                                                                                                   \
        if (order == sort_order::ascending)                                                             \
            hwy::VQSelect((H*)keys, n, k, hwy::SortAscending());                                        \
        else                                                                                            \
            hwy::VQSelect((H*)keys, n, k, hwy::SortDescending());                                       \
    }

FM_VQSORT(uint16_t, uint16_t)
FM_VQSORT(int16_t, int16_t)
FM_VQSORT(uint32_t, uint32_t)
FM_VQSORT(int32_t, int32_t)
FM_VQSORT(uint64_t, uint64_t)
FM_VQSORT(int64_t, int64_t)
FM_VQSORT(float, float)
FM_VQSORT(double, double)
FM_VQSORT(uint128_t, hwy::uint128_t)
FM_VQSORT(K64V64, hwy::K64V64)
FM_VQSORT(K32V32, hwy::K32V32)

#undef FM_VQSORT

} // namespace floormat
