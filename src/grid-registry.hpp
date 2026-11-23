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
};

} // namespace floormat::Grid
