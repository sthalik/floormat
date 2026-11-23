#include "grid-registry.hpp"
#include "world.hpp"
#include "compat/thread-pool.hpp"
#include <cr/GrowableArray.h>

namespace floormat::Grid {

GridRegistry::GridRegistry()
{
    arrayReserve(pass_.pools_in_creation_order, 16);
    arrayReserve(object_.pools_in_creation_order, 16);
    arrayReserve(grids_to_build_, 16);
    arrayReserve(cover_grids_to_fill_, 16);
}

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
    const auto add_in_creation_order = [&]([[maybe_unused]] auto& pool) {
        if constexpr (requires { pools.pools_in_creation_order; })
            arrayAppend(pools.pools_in_creation_order, &pool);
    };
    if (auto* slot = find_pool_slot(pools, pʹ.validate()))
    {
        if (!*slot)
        {
            slot->emplace(forward<Args>(args)...);
            add_in_creation_order(**slot);
        }
        return **slot;
    }
    else
    {
        arrayReserve(pools.fallback, 4);
        arrayAppend(pools.fallback, InPlaceInit, InPlaceInit, forward<Args>(args)...);
        add_in_creation_order(*pools.fallback.back());
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

u32 GridRegistry::pool_count() const
{
    auto n = (u32)(pass_.pools_in_creation_order.size() + object_.pools_in_creation_order.size() +
                   cover_.fallback.size());
    for (const auto& p : cover_.direct)
        n += !!p;
    return n;
}

void GridRegistry::list_requested_stale_grids()
{
    for (auto* pool : pass_.pools_in_creation_order)
        pool->list_requested_stale_grids(grids_to_build_);
    for (auto* pool : object_.pools_in_creation_order)
        pool->list_requested_stale_grids(grids_to_build_);
}

void GridRegistry::list_pass_grid(const Pass::Params& p, chunk& c)
{
    pass_pool(p).list_grid(c, grids_to_build_);
}

void GridRegistry::list_object_grid(const Object::Params& p, chunk& c)
{
    auto& pool = object_pool(p);
    pool.list_grid(c, grids_to_build_);
    pool.pass_pool().list_grid(c, grids_to_build_);
}

u32 GridRegistry::list_cover_octant(const Cover::Params& p, chunk& c, u32 k, u32 max_octant_fills)
{
    return cover_pool(p).list_octant(c, k, max_octant_fills, cover_grids_to_fill_, grids_to_build_);
}

void GridRegistry::build_listed_grids()
{
    if (grids_to_build_.isEmpty())
        return;
    auto& w = grids_to_build_[0].grid->c->world();
    for (const auto& x : grids_to_build_)
        fm_debug_assert(&x.grid->c->world() == &w);

    auto* pool = w.thread_pool();
    const auto num_scratch_arrays = pool ? pool->num_threads() : 1u;
    if (const auto size = (u32)thread_scratch_runs_.size(); size < num_scratch_arrays)
    {
        arrayResize(thread_scratch_runs_, num_scratch_arrays);
        for (auto i = size; i < num_scratch_arrays; i++)
            arrayReserve(thread_scratch_runs_[i], 16);
    }

    if (pool)
    {
        const auto count = (u32)grids_to_build_.size();
        w.set_running_job_kind(job_kind::build_grids);
        pool->parallel_for(count, pool->default_grain_size(count), [this](u32 begin, u32 end, u32 worker_index) {
            auto& scratch_runs = thread_scratch_runs_[worker_index];
            for (auto i = begin; i < end; i++)
                grids_to_build_[i].build_grid(grids_to_build_[i].grid, scratch_runs);
        });
        w.set_running_job_kind(job_kind::make_critters_think);
    }
    else
        for (const auto& x : grids_to_build_)
            x.build_grid(x.grid, thread_scratch_runs_[0]);

    for (const auto& x : grids_to_build_)
    {
        x.grid->build_no = detail::GridBase::next_build_no();
        x.grid->is_listed_to_be_built = false;
    }
    arrayClear(grids_to_build_);
}

void GridRegistry::fill_listed_cover_octants()
{
    if (cover_grids_to_fill_.isEmpty())
        return;
    fm_debug_assert(grids_to_build_.isEmpty());
    auto& w = cover_grids_to_fill_[0].grid->c->world();
    for (const auto& x : cover_grids_to_fill_)
        fm_debug_assert(&x.grid->c->world() == &w);

    if (auto* pool = w.thread_pool())
    {
        w.set_running_job_kind(job_kind::fill_cover_octants);
        pool->parallel_for((u32)cover_grids_to_fill_.size(), 1, [this](u32 begin, u32 end, u32) {
            for (auto i = begin; i < end; i++)
                detail::fill_listed_octants(cover_grids_to_fill_[i]);
        });
        w.set_running_job_kind(job_kind::make_critters_think);
    }
    else
        for (auto& x : cover_grids_to_fill_)
            detail::fill_listed_octants(x);

    for (const auto& x : cover_grids_to_fill_)
        detail::add_listed_octant_fills(x);
    arrayClear(cover_grids_to_fill_);
}

void GridRegistry::rebuild_requested_stale_grids()
{
    list_requested_stale_grids();
    build_listed_grids();
}

} // namespace floormat::Grid
