#include "hash-table-load-factor.hpp"
#include "debug.hpp"
#include "array-size.hpp"
#include <algorithm>

namespace floormat::Hash {

namespace {

struct Entry
{
    float load_factor;
    size_t max_capacity;
};

constexpr Entry open_addressing_load_factor_table[] = {
    { 0.80f,          32},
    { 0.75f,         250},
    { 0.70f,       2'500},
    { 0.60f,      25'000},
    { 0.55f,     250'000},
    { 0.50f,   2'500'000},
    { 0.45f,  10'000'000},
    { 0.40f, 100'000'000},
};

constexpr Entry separate_chaining_load_factor_table[] = {
    { 1.500f,          32 }, // 2^5 (Fits in L1 cache)
    { 1.250f,         256 }, // 2^8 (Fits in L1 cache)
    { 1.000f,       4'096 }, // 2^12 (Spilling to L2 cache)
    { 0.875f,      65'536 }, // 2^16 (Spilling to L3 cache)
    { 0.750f,   1'048'576 }, // 2^20 (Spilling to Main RAM)
    { 0.650f, 268'435'456 }, // 2^28 (Spilling to Main RAM)
};

constexpr float get_value(size_t element_count, const auto& table)
{
    size_t i{};
#if 0
    auto it = std::lower_bound(
        table, table + array_size(table), element_count,
        [](const Entry a, size_t b) {
            return a.max_capacity < b;
        });
    i = (size_t)std::distance(table, it);
#else
    for (i = 0; i < array_size(table); i++)
        if (element_count <= table[i].max_capacity)
            break;
#endif
    i = std::min(i, array_size(table)-1);
    return table[i].load_factor;
}

constexpr float test(size_t element_count) {
    return get_value(element_count, open_addressing_load_factor_table);
}

static_assert(test(          0) == 0.80f);
static_assert(test(         32) == 0.80f);
static_assert(test(         33) == 0.75f);
static_assert(test(        250) == 0.75f);
static_assert(test(        251) == 0.70f);
static_assert(test( 90'000'000) == 0.40f);
static_assert(test(100'000'000) == 0.40f);
static_assert(test(100'000'001) == 0.40f);
static_assert(test((size_t)-1)  == 0.40f);

} // namespace

float open_addressing_load_factor(size_t element_count) {
    auto ret = get_value(element_count, open_addressing_load_factor_table);
    //DBG << "open" << ret;
    return ret;
}

float separate_chaining_load_factor(size_t element_count) {
    auto ret = get_value(element_count, separate_chaining_load_factor_table);
    //DBG << "chaining" << ret;
    return ret;
}

} // namespace floormat::Hash
