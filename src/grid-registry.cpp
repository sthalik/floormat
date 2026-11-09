#include "grid-registry.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid {

GridRegistry::GridRegistry() = default;
GridRegistry::~GridRegistry() noexcept = default;

Pass::Pool& GridRegistry::pass_pool(const Pass::Params& pʹ, Kind kind)
{
    fm_assert(kind < Kind::COUNT);
    const auto p = pʹ.validate();
    const auto div_size = p.div_size();
    const auto& predicate = kind == Kind::static_ ? Search::without_dynamic_objects() : Search::dynamic_scenery();
    auto& pass = pass_[(u32)kind];
    // The table is indexed by countr_zero(div_size), which gives two different sizes the same
    // slot unless div_size is a power of two. Other sizes take the linear search below.
    if (p.bbox_size <= direct_max_bbox && std::has_single_bit(div_size))
    {
        const auto mode = (u32)p.min_pass - 1;
        const auto div_shift = (u32)std::countr_zero(div_size);
        auto& slot = pass.direct[mode][div_shift][p.bbox_size - Pass::Grid::min_bbox_size];
        if (!slot)
            slot.emplace(p, predicate);
        return *slot;
    }

    for (auto& pool : pass.fallback)
        if (pool->params() == p)
            return *pool;
    arrayReserve(pass.fallback, 4);
    arrayAppend(pass.fallback, InPlaceInit, InPlaceInit, p, predicate);
    return *pass.fallback.back();
}

Cover::Pool& GridRegistry::cover_pool(const Cover::Params& pʹ, Kind kind)
{
    fm_assert(kind < Kind::COUNT);
    const auto p = pʹ.validate();
    const Pass::Params pass_params{p.div_size, pass_mode::shoot_through};
    auto& cover = cover_[(u32)kind];
    // As in pass_pool(), countr_zero(div_size) is an exact index only for a power-of-two div_size.
    if (std::has_single_bit(p.div_size))
    {
        auto& slot = cover.direct[(u32)std::countr_zero(p.div_size)];
        if (!slot)
            slot.emplace(p, pass_pool(pass_params, kind));
        return *slot;
    }

    for (auto& pool : cover.fallback)
        if (pool->params() == p)
            return *pool;
    arrayReserve(cover.fallback, 4);
    arrayAppend(cover.fallback, InPlaceInit, InPlaceInit, p, pass_pool(pass_params, kind));
    return *cover.fallback.back();
}

} // namespace floormat::Grid
