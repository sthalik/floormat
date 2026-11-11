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

constexpr Params pool_params[] = {
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

template<typename Pool>
void check_pool_per_params(Pool& (Grid::GridRegistry::*get)(const Params&))
{
    constexpr auto N = array_size(pool_params);
    Grid::GridRegistry reg;
    Pool* pools[N];
    for (auto& [i, pool] : enumerate(pools))
    {
        pool = &(reg.*get)(pool_params[i]);
        fm_assert(pool->params() == pool_params[i]);
    }
    for (const auto& [i, pool] : enumerate(pools))
        fm_assert(&(reg.*get)(pool_params[i]) == pool);
    for (const auto& [a, b] : combinations(pools))
        fm_assert(a != b);
}

void test_pools_snap_params()
{
    Grid::GridRegistry reg;
    // div 16 snaps to 8 at bbox 10
    constexpr auto a = Params{16, 10, pass_mode::pass}, b = Params{8, 10, pass_mode::pass};
    fm_assert(&reg.pass_pool(a) == &reg.pass_pool(b));
    fm_assert(&reg.object_pool(a) == &reg.object_pool(b));
}

void test_cover_pool_per_div()
{
    constexpr auto N = (u32)std::bit_width(Grid::Cover::max_div_size);
    Grid::GridRegistry reg;
    Grid::Cover::Pool* pools[N];
    for (auto& [i, pool] : enumerate(pools))
    {
        pool = &reg.cover_pool(Grid::Cover::Params{1u << i});
        fm_assert(pool->params() == Grid::Cover::Params{1u << i});
    }
    for (const auto& [i, pool] : enumerate(pools))
        fm_assert(&reg.cover_pool(Grid::Cover::Params{1u << i}) == pool);
    for (const auto& [a, b] : combinations(pools))
        fm_assert(a != b);
}

void test_cover_pool_shares_pass_pool()
{
    auto w = world();
    auto& c = w[chunk_coords_{0, 0, 0}];
    Grid::GridRegistry reg;
    auto& cover = reg.cover_pool(Grid::Cover::Params{8});
    cover[c].build_if_stale();
    fm_assert(reg.pass_pool(Params{8, pass_mode::shoot_through})[c].build_no() != 0);
    // a grid this pool never built reads 0
    fm_assert(reg.pass_pool(Params{8, pass_mode::see_through})[c].build_no() == 0);
}

} // namespace

void Test::test_pool_registry()
{
    check_pool_per_params(&Grid::GridRegistry::pass_pool);
    check_pool_per_params(&Grid::GridRegistry::object_pool);
    test_pools_snap_params();
    test_cover_pool_per_div();
    test_cover_pool_shares_pass_pool();
}

} // namespace floormat
