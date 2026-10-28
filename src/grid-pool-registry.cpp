#include "grid-pool-registry.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid {

PoolRegistry::PoolRegistry() = default;
PoolRegistry::~PoolRegistry() noexcept = default;

Pass::Pool& PoolRegistry::pass_pool(const Pass::Params& p)
{
    // The table holds only pass pools, so a slot's div_size and bbox_size give its params
    // and equal params always find the same pool.
    if (p.bbox_size <= direct_max_bbox && p.min_pass == pass_mode::pass)
    {
        const auto div_shift = (uint32_t)std::countr_zero(p.validate().div_size()); // xxx todo assumes power of two chunk size
        auto& slot = direct_[div_shift][p.bbox_size - Pass::Grid::min_bbox_size];
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

Cover::Pool& PoolRegistry::cover_pool(const Cover::Params& p)
{
    auto& slot = cover_[(uint32_t)std::countr_zero(p.validate().div_size)]; // xxx todo assumes power of two chunk size
    if (!slot)
        slot.emplace(p, pass_pool(Pass::Params{p.div_size, pass_mode::shoot_through}));
    return *slot;
}

} // namespace floormat::Grid
