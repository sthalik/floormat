#pragma once

namespace floormat {

enum class sort_order : bool { ascending, descending };

// Same layouts as Highway's types of the same name. uint128_t orders by (hi, lo), the other two by key alone.
struct alignas(16) uint128_t { uint64_t lo, hi; };
struct alignas(16) K64V64 { uint64_t value, key; };
struct alignas(8) K32V32 { uint32_t value, key; };

// Equal keys may come out in any order.
void vqsort(uint16_t* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(int16_t* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(uint32_t* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(int32_t* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(uint64_t* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(int64_t* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(float* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(double* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(uint128_t* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(K64V64* keys, uint32_t n, sort_order order = sort_order::ascending);
void vqsort(K32V32* keys, uint32_t n, sort_order order = sort_order::ascending);

void vqsort_partial(uint16_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(int16_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(uint32_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(int32_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(uint64_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(int64_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(float* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(double* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(uint128_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(K64V64* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqsort_partial(K32V32* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);

void vqselect(uint16_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(int16_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(uint32_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(int32_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(uint64_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(int64_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(float* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(double* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(uint128_t* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(K64V64* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);
void vqselect(K32V32* keys, uint32_t n, uint32_t k, sort_order order = sort_order::ascending);

} // namespace floormat
