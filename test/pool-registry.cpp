#include "app.hpp"
#include "compat/array-size.hpp"
#include "src/grid-pool-registry.hpp"
#include "src/world.hpp"
#include "src/chunk.hpp"
#include <bit>

namespace floormat {

namespace {

using Pass::Params;

void test_pass_pool_per_params()
{
    constexpr Params params[] = {
        {16, 32, pass_mode::pass},
        {16, 32, pass_mode::shoot_through},
        {8, 32, pass_mode::pass},
        {1, 4, pass_mode::pass},
        // off the table: odd, and over direct_max_bbox
        {16, 33, pass_mode::pass},
        {16, 300, pass_mode::pass},
    };
    constexpr auto N = array_size(params);

    Grid::PoolRegistry reg{16};
    Pass::Pool* pools[N];
    for (auto i = 0u; i < N; i++)
    {
        pools[i] = &reg.pass_pool(params[i]);
        fm_assert(pools[i]->params() == params[i]);
    }
    for (auto i = 0u; i < N; i++)
    {
        fm_assert(&reg.pass_pool(params[i]) == pools[i]);
        for (auto j = i + 1; j < N; j++)
            fm_assert(pools[i] != pools[j]);
    }
}

void test_pool_for_is_pass_pool()
{
    Grid::PoolRegistry reg{16};
    for (uint32_t bbox : { 1u, 4u, 5u, 32u, 254u, 255u, 300u })
    {
        auto& pool = reg.pool_for(bbox);
        fm_assert(&reg.pass_pool(pool.params()) == &pool);
    }
    // div 16 snaps to 8 at bbox 10
    fm_assert(&reg.pass_pool(Params{8, 10, pass_mode::pass}) == &reg.pool_for(10));
}

void test_cover_pool_per_div()
{
    constexpr auto N = (uint32_t)std::bit_width(chunk_size_xy);
    Grid::PoolRegistry reg{16};
    Cover::Pool* pools[N];
    for (auto i = 0u; i < N; i++)
    {
        pools[i] = &reg.cover_pool(Cover::Params{1u << i});
        fm_assert(pools[i]->params() == Cover::Params{1u << i});
    }
    for (auto i = 0u; i < N; i++)
    {
        fm_assert(&reg.cover_pool(Cover::Params{1u << i}) == pools[i]);
        for (auto j = i + 1; j < N; j++)
            fm_assert(pools[i] != pools[j]);
    }
}

void test_cover_pool_shares_pass_pool()
{
    auto w = world();
    auto& c = w[chunk_coords_{0, 0, 0}];
    Grid::PoolRegistry reg{16};
    auto& cover = reg.cover_pool(Cover::Params{8});
    cover.maybe_mark_stale_all(w);
    cover[c].build_if_stale();
    // a grid this pool never built reads 0
    fm_assert(reg.pass_pool(Params{8, pass_mode::shoot_through})[c].build_no() != 0);
}

} // namespace

void Test::test_pool_registry()
{
    test_pass_pool_per_params();
    test_pool_for_is_pass_pool();
    test_cover_pool_per_div();
    test_cover_pool_shares_pass_pool();
}

} // namespace floormat
