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

    template<typename Pool, typename... Args>
    static Pool& find_or_add(pools_by_params<Pool>& pools, const Pass::Params& p, Args&&... args);

    pools_by_params<Pass::Pool> pass_;
    pools_by_params<Object::Pool> object_;
    // after the pass pools, so destroyed first: each borrows one of them
    struct
    {
        std::array<Pointer<Cover::Pool>, std::bit_width(Cover::max_div_size)> direct;
        Array<Pointer<Cover::Pool>> fallback;
    } cover_;
};

} // namespace floormat::Grid
