#include "grid-pool-registry.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid {

PoolRegistry::PoolRegistry() = default;
PoolRegistry::~PoolRegistry() noexcept = default;

Pass::Pool& PoolRegistry::pass_pool(const Pass::Params& pʹ)
{
    const auto p = pʹ.validate();
    const auto div_size = p.div_size();
    // The table is indexed by countr_zero(div_size), which gives two different sizes the same
    // slot unless div_size is a power of two. Other sizes take the linear search below.
    if (p.bbox_size <= direct_max_bbox && std::has_single_bit(div_size))
    {
        const auto mode = (uint32_t)p.min_pass - 1;
        const auto div_shift = (uint32_t)std::countr_zero(div_size);
        auto& slot = pass_.direct[mode][div_shift][p.bbox_size - Pass::Grid::min_bbox_size];
        if (!slot)
            slot.emplace(p, Search::without_critters());
        return *slot;
    }

    for (auto& pool : pass_.fallback)
        if (pool->params() == p)
            return *pool;
    arrayAppend(pass_.fallback, InPlaceInit, InPlaceInit, p, Search::without_critters());
    return *pass_.fallback.back();
}

Cover::Pool& PoolRegistry::cover_pool(const Cover::Params& pʹ)
{
    const auto p = pʹ.validate();
    const Pass::Params pass_params{p.div_size, pass_mode::shoot_through};
    // As in pass_pool(), countr_zero(div_size) is an exact index only for a power-of-two div_size.
    if (std::has_single_bit(p.div_size))
    {
        auto& slot = cover_.direct[(uint32_t)std::countr_zero(p.div_size)];
        if (!slot)
            slot.emplace(p, pass_pool(pass_params));
        return *slot;
    }

    for (auto& pool : cover_.fallback)
        if (pool->params() == p)
            return *pool;
    arrayAppend(cover_.fallback, InPlaceInit, InPlaceInit, p, pass_pool(pass_params));
    return *cover_.fallback.back();
}

} // namespace floormat::Grid
