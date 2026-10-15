#include "multi-level-table-common.hpp"

namespace floormat {

using namespace floormat::mlt_test;

namespace {

static_assert(plane_table::dims == 3 && plane_table::key_bits == 7);
static_assert(packs_to<plane_table>({1, 0, 0}, 1) && packs_to<plane_table>({0, 1, 0}, 8) && packs_to<plane_table>({8, 0, 0}, 16));
static_assert(packs_to<plane_table>({0, 2, 0}, 64) && packs_to<plane_table>({31, 3, 0}, 127) && packs_to<plane_table>({5, 2, 0}, 69));
static_assert(packs_to<plane_deep_free_table>({0, 1, 0}, 4) && packs_to<plane_deep_free_table>({0, 2, 0}, 8) && packs_to<plane_deep_free_table>({0, 4, 0}, 16));
static_assert(packs_to<plane_deep_free_table>({4, 0, 0}, 32) && packs_to<plane_deep_free_table>({31, 7, 0}, 255));
static_assert(packs_to<plane_zero_table>({4, 0, 0}, 8) && packs_to<plane_zero_table>({8, 0, 0}, 16) && packs_to<plane_zero_table>({0, 2, 0}, 32));
static_assert(packs_to<plane_wide_table>({0, 1, 0}, 1 << 16) && packs_to<plane_wide_table>({0x10000, 0, 0}, 1 << 17));
static_assert(packs_to<plane_wide_table>({0xffffffff, 1, 0}, (uint64_t{1} << 33) - 1));
static_assert(packs_to<plane_large_table>({1, 0, 0}, 1) && packs_to<plane_large_table>({0, 1, 0}, 8) && packs_to<plane_large_table>({8, 0, 0}, 64));
static_assert(packs_to<plane_large_table>({0, 8, 0}, 128) && packs_to<plane_large_table>({16, 0, 0}, 256) && packs_to<plane_large_table>({0, 16, 0}, 1024));
static_assert(packs_to<plane_large_table>({63, 63, 0}, 4095) && top_bytes<plane_large_table> == 24 << 4);

void check_plane_wide()
{
    using Table = plane_wide_table;
    constexpr uint32_t xs[] = { 0, 1, 0xffff, 0x10000, 0x1ffff, 0xffffffff, };
    Table t;
    uint64_t keys[12];
    uint32_t n = 0;
    for (uint32_t y = 0; y < 2; y++)
        for (uint32_t x : xs)
        {
            fm_assert(t.insert(coord3{x, y, 0}, uint8_t(n + 1)));
            keys[n++] = Table::pack(coord3{x, y, 0});
        }
    fm_assert(t.page_count() == 3);
    check_layout(t, size_t{8} << 16, keys);
    n = 0;
    for (uint32_t y = 0; y < 2; y++)
        for (uint32_t x : xs)
        {
            fm_assert(t.find(coord3{x, y, 0}) == t.find(keys[n]) && *t.find(coord3{x, y, 0}) == n + 1);
            fm_assert(!t.insert(coord3{x, y, 0}, 1));
            n++;
        }
    fm_assert(t.find(coord3{2, 1, 0}) && !*t.find(coord3{2, 1, 0}) && t.find(coord3{0xfffffffe, 0, 0}) && !*t.find(coord3{0xfffffffe, 0, 0}));
    fm_assert(!t.find(coord3{0x20000, 0, 0}) && !t.find(coord3{0xfffeffff, 1, 0}));
    n = 0;
    for (uint32_t y = 0; y < 2; y++)
        for (uint32_t x : xs)
            fm_assert(t.erase(coord3{x, y, 0}) == ++n);
    fm_assert(t.page_count() == 3);
    check_layout(t, size_t{8} << 16, {});
}

} // namespace

template class multi_level_table<tracked, plane_params>;
template class multi_level_table<tracked, plane_deep_free_params>;
template class multi_level_table<tracked, plane_no_y_params>;
template class multi_level_table<tracked, plane_zero_params>;
template class multi_level_table<tracked, plane_zero_free_params>;
template class multi_level_table<tracked, plane_no_pages_params>;
template class multi_level_table<uint8_t, plane_wide_params>;
template class multi_level_table<tracked, plane_large_params>;

void mlt_test::test_plane_tables()
{
    check_plane_wide();

    check_pack<plane_table>(0);
    check_pack<plane_deep_free_table>(0);
    check_pack<plane_no_y_table>(0);
    check_pack<plane_zero_table>(0);
    check_pack<plane_zero_free_table>(0);
    check_pack<plane_no_pages_table>(0);
    check_pack<plane_wide_table>(89);
    check_pack<plane_large_table>(0);

    check_fill<plane_table>(63);
    check_fill<plane_deep_free_table>(64);
    check_fill<plane_no_y_table>(65);
    check_fill<plane_zero_table>(66);
    check_fill<plane_zero_free_table>(67);
    check_fill<plane_no_pages_table>(68);
    check_fill<plane_wide_table>(69);
    check_fill<plane_large_table>(91);
    fm_assert(tracked::live == 0);

    check_random<plane_table>(with(76, size_t{8} << 3));
    check_random<plane_deep_free_table>(with(77, size_t{16} << 5));
    check_random<plane_no_y_table>(with(78, size_t{8} << 3));
    check_random<plane_zero_table>(with(79, size_t{16} << 2));
    check_random<plane_zero_free_table>(with(80, size_t{24} << 2));
    check_random<plane_no_pages_table>(with(81, size_t{8} << 5));
    check_random<plane_wide_table>({.seed = 82, .ops = 200, .top_size_bytes = size_t{8} << 16, .check_every = 100, .page_cap = 8});
    fm_assert(tracked::live == 0);

    {
        using enum walk_type;
        check_walk<plane_large_table>(101, simple);
        check_walk<plane_deep_free_table>(104, levy_flight);
        check_walk<plane_table>(107, persistent);
        check_walk<plane_large_table>(110, kinetic_growth);
        check_walk<plane_zero_table>(113, true_self_avoiding);
        check_walk<plane_deep_free_table>(116, loop_erased);
        check_walk<plane_large_table>(119, vertex_reinforced);
        check_walk<plane_table>(122, elephant);
        check_walk<plane_large_table>(125, resetting);
        check_walk<plane_zero_table>(128, random_environment);
        check_walk<plane_deep_free_table>(131, excited);
        check_walk<plane_large_table>(134, langton_ant, 12000);
        check_walk<plane_table>(137, rotor_router);
        check_walk<plane_large_table>(140, internal_dla);
        check_walk<plane_deep_free_table>(143, eden);
        check_retrace<plane_large_table>(146, 8);
        check_walk<plane_deep_free_table>(149, page_corner_drift);
        check_retrace<plane_deep_free_table>(152, 4);
        check_retrace<plane_large_table>(155, 16);
        check_retrace<plane_large_table>(156, 32);
    }
    fm_assert(tracked::live == 0);
}

} // namespace floormat
