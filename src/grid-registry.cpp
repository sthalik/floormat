#include "grid-registry.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid {

GridRegistry::GridRegistry() = default;
GridRegistry::~GridRegistry() noexcept = default;

template<typename Pools>
auto GridRegistry::find_pool_slot(Pools& pools, const Pass::Params& p) -> decltype(&pools.direct[0][0][0])
{
    const auto div_size = p.div_size();
    // The table is indexed by countr_zero(div_size), which gives two different sizes the same
    // slot unless div_size is a power of two. Other sizes take the linear search below.
    if (p.bbox_size <= direct_max_bbox && std::has_single_bit(div_size))
    {
        const auto mode = (u32)p.min_pass - 1;
        const auto div_shift = (u32)std::countr_zero(div_size);
        return &pools.direct[mode][div_shift][p.bbox_size - Pass::Grid::min_bbox_size];
    }
    else
    {
        for (auto& pool : pools.fallback)
            if (pool->params() == p)
                return &pool;
        return nullptr;
    }
}

template<typename Pools>
auto GridRegistry::find_pool_slot(Pools& pools, const Cover::Params& p) -> decltype(&pools.direct[0])
{
    // As above, countr_zero(div_size) is an exact index only for a power-of-two div_size.
    if (std::has_single_bit(p.div_size))
        return &pools.direct[(u32)std::countr_zero(p.div_size)];
    else
    {
        for (auto& pool : pools.fallback)
            if (pool->params() == p)
                return &pool;
        return nullptr;
    }
}

template<typename Pools, typename Params, typename... Args>
auto& GridRegistry::find_or_add(Pools& pools, const Params& pʹ, Args&&... args)
{
    if (auto* slot = find_pool_slot(pools, pʹ.validate()))
    {
        if (!*slot)
            slot->emplace(forward<Args>(args)...);
        return **slot;
    }
    else
    {
        arrayReserve(pools.fallback, 4);
        arrayAppend(pools.fallback, InPlaceInit, InPlaceInit, forward<Args>(args)...);
        return *pools.fallback.back();
    }
}

Pass::Pool& GridRegistry::pass_pool(const Pass::Params& p)
{
    return find_or_add(pass_, p, p);
}

Object::Pool& GridRegistry::object_pool(const Object::Params& p)
{
    return find_or_add(object_, p, pass_pool(p));
}

Cover::Pool& GridRegistry::cover_pool(const Cover::Params& p)
{
    return find_or_add(cover_, p, p, pass_pool({p.div_size, pass_mode::shoot_through}));
}

const Pass::Pool* GridRegistry::find_pass_pool(const Pass::Params& p) const
{
    const auto* slot = find_pool_slot(pass_, p.validate());
    return slot && *slot ? &**slot : nullptr;
}

const Object::Pool* GridRegistry::find_object_pool(const Object::Params& p) const
{
    const auto* slot = find_pool_slot(object_, p.validate());
    return slot && *slot ? &**slot : nullptr;
}

const Cover::Pool* GridRegistry::find_cover_pool(const Cover::Params& p) const
{
    const auto* slot = find_pool_slot(cover_, p.validate());
    return slot && *slot ? &**slot : nullptr;
}

} // namespace floormat::Grid
