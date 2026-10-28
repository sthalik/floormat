#pragma once

#include "compat/defs.hpp"
#include "compat/limits.hpp"
#include "grid-pass.hpp"
#include "grid-cover.hpp"
#include <array>
#include <bit>
#include <cr/Pointer.h>
#include <cr/Array.h>

namespace floormat::Grid {

class PoolRegistry final
{
public:
    PoolRegistry();
    ~PoolRegistry() noexcept;
    fm_DISABLE_MOVE_COPY(PoolRegistry);

    Pass::Pool& pass_pool(const Pass::Params& p);
    Cover::Pool& cover_pool(const Cover::Params& p);

    // object bbox sizes are uint8_t
    static constexpr uint32_t direct_max_bbox = limits<uint8_t>::max;
    static constexpr uint32_t direct_bbox_count = direct_max_bbox - Pass::Grid::min_bbox_size + 1;
    // div_size is a power of two no larger than bbox_size, so countr_zero(div_size) is below this.
    static constexpr uint32_t direct_div_count = (uint32_t)std::bit_width(direct_max_bbox);

private:
    std::array<std::array<Pointer<Pass::Pool>, direct_bbox_count>, direct_div_count> direct_;
    Array<Pointer<Pass::Pool>> fallback_;
    // after the pass pools, so destroyed first: each borrows one of them
    std::array<Pointer<Cover::Pool>, std::countr_zero(chunk_size_xy)> cover_; // xxx todo assumes power of two chunk size
};

} // namespace floormat::Grid
