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
    // The pass_.direct branch takes only a power-of-two div_size no larger than bbox_size, so
    // countr_zero(div_size) is below this.
    static constexpr uint32_t direct_div_count = (uint32_t)std::bit_width(direct_max_bbox);
    // Params::validate() rejects pass_mode::blocked, so min_pass - 1 is below this.
    static constexpr uint32_t direct_mode_count = (uint32_t)pass_mode::COUNT - 1;

private:
    struct
    {
        std::array<std::array<std::array<Pointer<Pass::Pool>, direct_bbox_count>, direct_div_count>, direct_mode_count> direct;
        Array<Pointer<Pass::Pool>> fallback;
    } pass_;
    // after the pass pools, so destroyed first: each borrows one of them
    struct
    {
        std::array<Pointer<Cover::Pool>, std::countr_zero(chunk_size_xy)> direct; // xxx todo assumes power of two chunk size
        Array<Pointer<Cover::Pool>> fallback;
    } cover_;
};

} // namespace floormat::Grid
