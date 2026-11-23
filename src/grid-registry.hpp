#pragma once

#include "compat/defs.hpp"
#include "compat/limits.hpp"
#include "grid-pass.hpp"
#include "grid-cover.hpp"
#include "grid-object.hpp"
#include <array>
#include <bit>
#include <cr/Pointer.h>
#include <cr/Array.h>

namespace floormat::Grid {

class GridRegistry final
{
public:
    GridRegistry();
    ~GridRegistry() noexcept;
    fm_DISABLE_MOVE_COPY(GridRegistry);

    Pass::Pool& pass_pool(const Pass::Params& p);
    Cover::Pool& cover_pool(const Cover::Params& p);
    Object::Pool& object_pool(const Object::Params& p);

    // These return the pool for the params, or null when no call has created it. They write nothing.
    const Pass::Pool* find_pass_pool(const Pass::Params& p) const;
    const Object::Pool* find_object_pool(const Object::Params& p) const;
    const Cover::Pool* find_cover_pool(const Cover::Params& p) const;
    // The number of pass, object and cover pools made, all three kinds together.
    u32 pool_count() const;

    // Lists every stale pass grid and object grid that has is_requested_by_reader set, then clears the flag on
    // every pass and object grid. Cover grids are left to grid orders, which count their rebuild against the cap.
    void list_requested_stale_grids();
    // Lists c's grid in p's pool when it needs a rebuild and isn't listed. Makes the pool and the grid if missing.
    void list_pass_grid(const Pass::Params& p, chunk& c);
    // As list_pass_grid() for c's object grid, then for its pass grid of the same params.
    void list_object_grid(const Object::Params& p, chunk& c);
    // As Cover::Pool::list_octant() on p's pool, which it makes if missing, listing into this registry's lists.
    u32 list_cover_octant(const Cover::Params& p, chunk& c, u32 k, u32 max_octant_fills);
    // Builds the listed grids, as jobs of their world's thread pool if it has one, numbers them in list order and
    // empties the list. With a pool, every chunk's R-trees must be current, as after world::ensure_passability().
    void build_listed_grids();
    // As build_listed_grids(), for what list_cover_octant() listed, and adds the octant fills to their pools'
    // totals. Only a rebuilt grid gets a build_no. Call build_listed_grids() first.
    void fill_listed_cover_octants();
    // Builds what list_requested_stale_grids() lists.
    void rebuild_requested_stale_grids();

    // object bbox sizes are u8
    static constexpr u32 direct_max_bbox = limits<u8>::max;
    static constexpr u32 direct_bbox_count = direct_max_bbox - Pass::Grid::min_bbox_size + 1;
    // The direct branch takes only a power-of-two div_size no larger than bbox_size, so
    // countr_zero(div_size) is below this.
    static constexpr u32 direct_div_count = (u32)std::bit_width(direct_max_bbox);
    // Params::validate() rejects pass_mode::blocked, so min_pass - 1 is below this.
    static constexpr u32 direct_mode_count = (u32)pass_mode::COUNT - 1;

private:
    template<typename Pool>
    struct pools_by_params
    {
        std::array<std::array<std::array<Pointer<Pool>, direct_bbox_count>, direct_div_count>, direct_mode_count> direct;
        Array<Pointer<Pool>> fallback;
        // The pools of direct and fallback, so a loop over every pool skips direct's empty slots.
        Array<Pool*> pools_in_creation_order;
    };

    // Both return the slot for p's pool, empty until the pool is made, or null when p's pool belongs in the
    // fallback list and isn't in it.
    template<typename Pools>
    static auto find_pool_slot(Pools& pools, const Pass::Params& p) -> decltype(&pools.direct[0][0][0]);
    template<typename Pools>
    static auto find_pool_slot(Pools& pools, const Cover::Params& p) -> decltype(&pools.direct[0]);

    template<typename Pools, typename Params, typename... Args>
    static auto& find_or_add(Pools& pools, const Params& p, Args&&... args);

    pools_by_params<Pass::Pool> pass_;
    // after the pass pools, so destroyed first: each borrows one of them
    pools_by_params<Object::Pool> object_;
    struct
    {
        std::array<Pointer<Cover::Pool>, std::bit_width(Cover::max_div_size)> direct;
        Array<Pointer<Cover::Pool>> fallback;
    } cover_;
    Array<detail::grid_to_build> grids_to_build_;
    Array<detail::cover_grid_to_fill> cover_grids_to_fill_;
    // Indexed by the worker_index of a build_listed_grids() job.
    Array<Array<u32>> thread_scratch_runs_;
};

} // namespace floormat::Grid
