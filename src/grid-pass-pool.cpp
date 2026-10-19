#include "grid-pass-pool.hpp"
#include "compat/round-to-even.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid::Pass {

PoolRegistry::PoolRegistry(uint32_t div_size) : div_size_{div_size} {}
PoolRegistry::~PoolRegistry() noexcept = default;

Pool& PoolRegistry::pass_pool(const Params& p)
{
    // the table holds only pool_for()'s params, so equal params always find the same pool
    if (p.bbox_size % 2 == 0 && p.bbox_size <= direct_max_bbox && p == Params{div_size_, p.bbox_size, pass_mode::pass})
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

Pool& PoolRegistry::pool_for(uint32_t bbox_size)
{
    fm_assert(bbox_size > 0);
    return pass_pool(Params{div_size_, round_to_even(bbox_size, bbox_size), pass_mode::pass});
}

} // namespace floormat::Grid::Pass
