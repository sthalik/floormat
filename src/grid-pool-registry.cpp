#include "grid-pool-registry.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid {

PoolRegistry::PoolRegistry() = default;
PoolRegistry::~PoolRegistry() noexcept = default;

Pass::Pool& PoolRegistry::pass_pool(const Pass::Params& p)
{
    p.validate();
    const auto div_size = p.div_size();
    // The table is indexed by countr_zero(div_size), which gives two different sizes the same
    // slot unless div_size is a power of two. Other sizes take the linear search below.
    if (p.bbox_size <= direct_max_bbox && std::has_single_bit(div_size))
    {
        const auto mode = (uint32_t)p.min_pass - 1;
        const auto div_shift = (uint32_t)std::countr_zero(div_size);
        auto& slot = direct_[mode][div_shift][p.bbox_size - Pass::Grid::min_bbox_size];
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
