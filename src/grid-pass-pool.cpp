#include "grid-pass-pool.hpp"
#include "compat/round-to-even.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid {

PoolRegistry::PoolRegistry(uint32_t div_size) : div_size_{div_size} {}
PoolRegistry::~PoolRegistry() noexcept = default;

Pass::Pool& PoolRegistry::pass_pool(const Pass::Params& p)
{
    // the table holds only pool_for()'s params, so equal params always find the same pool
    if (p.bbox_size % 2 == 0 && p.bbox_size <= direct_max_bbox && p == Pass::Params{div_size_, p.bbox_size, pass_mode::pass})
    {
        auto& slot = direct_[(p.bbox_size - 2u) / 2u];
        if (!slot)
            slot.emplace(p, Search::without_critters());
        return *slot;
    }

    for (auto& pool : fallback_)
        if (pool->params() == p)
            return *pool;
    arrayAppend(fallback_, InPlaceInit, InPlaceInit, p, Search::without_critters());
    return *fallback_.back();
}

Pass::Pool& PoolRegistry::pool_for(uint32_t bbox_size)
{
    fm_assert(bbox_size > 0);
    return pass_pool(Pass::Params{div_size_, round_to_even(bbox_size, bbox_size), pass_mode::pass});
}

Cover::Pool& PoolRegistry::cover_pool(const Cover::Params& p)
{
    auto& slot = cover_[(uint32_t)std::countr_zero(p.validate().div_size)];
    if (!slot)
        slot.emplace(p, pass_pool(Pass::Params{p.div_size, pass_mode::shoot_through}));
    return *slot;
}

} // namespace floormat::Grid
