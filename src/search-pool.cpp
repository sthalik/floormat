#include "search-pool.hpp"
#include "compat/round-to-even.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid::Pass {

PoolRegistry::PoolRegistry(): PoolRegistry(tile_size_xy) {}
PoolRegistry::PoolRegistry(uint32_t div_size) : div_size_{div_size} {}
PoolRegistry::~PoolRegistry() noexcept = default;

Pool& PoolRegistry::pool_for(uint32_t bbox_size)
{
    auto bbox = round_to_even(bbox_size, bbox_size);
    if (bbox < min_bbox_size)
        bbox = min_bbox_size;

    if (bbox <= direct_max_bbox)
    {
        const auto idx = (bbox - 2u) / 2u;
        auto& slot = direct_[idx];
        if (!slot)
        {
            slot.emplace(Params{div_size_, bbox});
            slot->maybe_mark_stale_all(last_frame_no_);
        }
        return *slot;
    }

    for (auto& p : fallback_)
        if (p->params().bbox_size == bbox)
            return *p;
    arrayAppend(fallback_, InPlaceInit, InPlaceInit, Params{tile_size_xy, bbox});
    fallback_.back()->maybe_mark_stale_all(last_frame_no_);
    return *fallback_.back();
}

void PoolRegistry::maybe_mark_stale_all(uint64_t frame_no)
{
    last_frame_no_ = frame_no;
    for (auto& p : direct_)
        if (p)
            p->maybe_mark_stale_all(frame_no);
    for (auto& p : fallback_)
        p->maybe_mark_stale_all(frame_no);
}

void PoolRegistry::build_if_stale_all()
{
    for (auto& p : direct_)
        if (p)
            p->build_if_stale_all();
    for (auto& p : fallback_)
        p->build_if_stale_all();
}

uint32_t PoolRegistry::live_pool_count() const
{
    uint32_t n = 0;
    for (const auto& p : direct_)
        if (p)
            ++n;
    n += (uint32_t)fallback_.size();
    return n;
}

} // namespace floormat::Grid::Pass
