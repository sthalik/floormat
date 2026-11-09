#include "app.hpp"
#include "compat/array-size.hpp"
#include "compat/combinations.hpp"
#include "compat/enumerate.hpp"
#include "src/grid-registry.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include <bit>

namespace floormat {

namespace {

using Pass::Params;
using Grid::Kind;

constexpr Kind kinds[] = { Kind::static_, Kind::dynamic };
static_assert(array_size(kinds) == (u32)Kind::COUNT);

void test_pass_pool_per_params()
{
    constexpr Params params[] = {
        {16, 32, pass_mode::pass},
        {16, 32, pass_mode::shoot_through},
        {16, 32, pass_mode::see_through},
        {8, 32, pass_mode::pass},
        {1, 4, pass_mode::pass},
        {1, 4, pass_mode::see_through},
        {16, 33, pass_mode::pass},
        {128, 255, pass_mode::pass},
        // off the table: over direct_max_bbox
        {16, 300, pass_mode::pass},
    };
    constexpr auto N = array_size(params);

    Grid::GridRegistry reg;
    Pass::Pool* pools[N * array_size(kinds)];
    for (auto& [i, pool] : enumerate(pools))
    {
        pool = &reg.pass_pool(params[i % N], kinds[i / N]);
        fm_assert(pool->params() == params[i % N]);
    }
    for (const auto& [i, pool] : enumerate(pools))
        fm_assert(&reg.pass_pool(params[i % N], kinds[i / N]) == pool);
    for (const auto& [a, b] : combinations(pools))
        fm_assert(a != b);
}

void test_pass_pool_snapped_params()
{
    Grid::GridRegistry reg;
    // div 16 snaps to 8 at bbox 10
    for (auto kind : kinds)
        fm_assert(&reg.pass_pool(Params{16, 10, pass_mode::pass}, kind) == &reg.pass_pool(Params{8, 10, pass_mode::pass}, kind));
}

void test_cover_pool_per_div()
{
    constexpr auto N = (u32)std::bit_width(Grid::Cover::max_div_size);
    Grid::GridRegistry reg;
    Grid::Cover::Pool* pools[N * array_size(kinds)];
    for (auto& [i, pool] : enumerate(pools))
    {
        pool = &reg.cover_pool(Grid::Cover::Params{1u << i % N}, kinds[i / N]);
        fm_assert(pool->params() == Grid::Cover::Params{1u << i % N});
    }
    for (const auto& [i, pool] : enumerate(pools))
        fm_assert(&reg.cover_pool(Grid::Cover::Params{1u << i % N}, kinds[i / N]) == pool);
    for (const auto& [a, b] : combinations(pools))
        fm_assert(a != b);
}

void test_cover_pool_shares_pass_pool()
{
    for (auto kind : kinds)
    {
        auto w = world();
        auto& c = w[chunk_coords_{0, 0, 0}];
        Grid::GridRegistry reg;
        auto& cover = reg.cover_pool(Grid::Cover::Params{8}, kind);
        cover.maybe_mark_stale_all(w);
        cover[c].build_if_stale();
        // a grid this pool never built reads 0
        fm_assert(reg.pass_pool(Params{8, pass_mode::shoot_through}, kind)[c].build_no() != 0);
        for (auto other : kinds)
            if (other != kind)
                fm_assert(reg.pass_pool(Params{8, pass_mode::shoot_through}, other)[c].build_no() == 0);
    }
}

} // namespace

void Test::test_pool_registry()
{
    test_pass_pool_per_params();
    test_pass_pool_snapped_params();
    test_cover_pool_per_div();
    test_cover_pool_shares_pass_pool();
}

} // namespace floormat
