#include "app.hpp"
#include "compat/array-size.hpp"
#include "src/grid-pass-pool.hpp"

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

    Pass::PoolRegistry reg{16};
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
    Pass::PoolRegistry reg{16};
    for (uint32_t bbox : { 1u, 4u, 5u, 32u, 254u, 255u, 300u })
    {
        auto& pool = reg.pool_for(bbox);
        fm_assert(&reg.pass_pool(pool.params()) == &pool);
    }
    // div 16 snaps to 8 at bbox 10
    fm_assert(&reg.pass_pool(Params{8, 10, pass_mode::pass}) == &reg.pool_for(10));
}

} // namespace

void Test::test_pool_registry()
{
    test_pass_pool_per_params();
    test_pool_for_is_pass_pool();
}

} // namespace floormat
