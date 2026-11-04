#pragma once

namespace floormat {

enum class sort_order : bool { ascending, descending };

// Same layouts as Highway's types of the same name. uint128_t orders by (hi, lo), the other two by key alone.
struct alignas(16) uint128_t { u64 lo, hi; };
struct alignas(16) K64V64 { u64 value, key; };
struct alignas(8) K32V32 { u32 value, key; };

// Equal keys may come out in any order.
void vqsort(u16* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(i16* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(u32* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(i32* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(u64* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(i64* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(float* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(double* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(uint128_t* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(K64V64* keys, u32 n, sort_order order = sort_order::ascending);
void vqsort(K32V32* keys, u32 n, sort_order order = sort_order::ascending);

void vqsort_partial(u16* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(i16* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(u32* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(i32* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(u64* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(i64* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(float* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(double* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(uint128_t* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(K64V64* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqsort_partial(K32V32* keys, u32 n, u32 k, sort_order order = sort_order::ascending);

void vqselect(u16* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(i16* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(u32* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(i32* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(u64* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(i64* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(float* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(double* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(uint128_t* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(K64V64* keys, u32 n, u32 k, sort_order order = sort_order::ascending);
void vqselect(K32V32* keys, u32 n, u32 k, sort_order order = sort_order::ascending);

// Call after hwy::SetSupportedTargetsForTest(); the target check is cached.
void vqsort_refresh_targets();
bool vqsort_avx2_or_better();

} // namespace floormat
