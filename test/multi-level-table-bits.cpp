#include "multi-level-table-common.hpp"

namespace floormat {

using namespace floormat::mlt_test;

template class multi_level_table<tracked, bit_page_params>;
template class multi_level_table<tracked, bit_flat_params>;
template class multi_level_table<uint32_t, bit_flat_params>;
template class multi_level_table<tracked, bit_pair_params>;
template class multi_level_table<tracked, bit_split_params>;
template class multi_level_table<tracked, bit_triple_params>;
template class multi_level_table<tracked, bit_deep_params>;
template class multi_level_table<tracked, wide_top_params>;
template class multi_level_table<tracked, narrow_top_params>;
template class multi_level_table<tracked, bit_zero_params>;
template class multi_level_table<tracked, wide_zero_params>;
template class multi_level_table<tracked, zero_top_params>;
template class multi_level_table<tracked, bit_split_free_params>;

void mlt_test::test_bit_tables()
{
    check_fill<bit_page_table>(49);
    check_fill<bit_flat_table>(50);
    check_fill<bit_flat_int_table>(51);
    check_fill<bit_pair_table>(52);
    check_fill<bit_split_table>(53);
    check_fill<bit_triple_table>(54);
    check_fill<bit_deep_table>(55);
    check_fill<wide_top_table>(56);
    check_fill<narrow_top_table>(57);
    check_fill<bit_zero_table>(58);
    check_fill<wide_zero_table>(59);
    check_fill<zero_top_table>(60);
    check_fill<bit_split_free_table>(62);
    fm_assert(tracked::live == 0);

    check_random<bit_page_table>(with(20, 16));
    check_random<bit_flat_table>(with(21, 16));
    check_random<bit_flat_int_table>(with(22, 8));
    check_random<bit_pair_table>(with(23, 16));
    check_random<bit_split_table>(with(24, 16));
    check_random<bit_triple_table>(with(25, 32));
    check_random<bit_deep_table>(with(26, 128));
    check_random<wide_top_table>(with(27, 256));
    check_random<narrow_top_table>(with(28, 16));
    check_random<bit_zero_table>(with(29, 128));
    check_random<wide_zero_table>(with(30, 32));
    check_random<zero_top_table>(with(31, 16));
    check_random<bit_split_free_table>(with(32, 48));
    fm_assert(tracked::live == 0);
}

} // namespace floormat
